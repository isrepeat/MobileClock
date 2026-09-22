#include "PageManager.h"

#include <Helpers.Logging/Logging.h>
#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/IRuntimeReloadableControl.h>
#include <XamlRuntime/RuntimeMarkup/RuntimeReloadTransaction.h>
#include <XamlRuntime/RuntimeMarkup/XamlParser.h>
#endif
#include <XamlRuntime/RenderEngine.h>

#include "MobileClock.Presentation/Renderer/AnimationRenderers.h"
#include "MobileClock.Presentation/Core/PageTransition.h"
#include "MobileClock.Presentation/Core/Registrations.h"

#include "NavigationStates.h"

#include <algorithm>
#include <format>
#include <limits>
#include <vector>
#include <array>

#ifndef MOBILECLOCK_ENABLE_PREVIEW_EDIT_ALARM_FALLBACK
#define MOBILECLOCK_ENABLE_PREVIEW_EDIT_ALARM_FALLBACK 0
#endif

namespace mobileclock::application::core {
    PageManager::PageManager(AppSessionController& appSessionController, model::AlarmRepository& alarmRepository, model::AlarmMelodyRepository& alarmMelodyRepository)
        : pageContext{static_cast<interface::IPageNavigator&>(*this), appSessionController, alarmRepository, alarmMelodyRepository}
        , pages(this->pageContext) {
    }

    //
    // IPageNavigator
    //
    bool PageManager::Navigate(std::string_view pageName) {
        interface::IPage* const page = this->pages.Find(pageName);
        if (page == nullptr || this->currentPage == page || this->isTransitioning) {
            return page != nullptr;
        }
        // Прямая навигация нужна previewer-у. Если запрошена предыдущая страница, она должна
        // сохранить ту же семантику, что и кнопка «Назад»: убрать текущую страницу из истории.
        if (this->navigationHistory.size() > 1 && this->navigationHistory[this->navigationHistory.size() - 2] == page) {
            if (!this->SwitchPage(*page, mobileclock::presentation::core::NavigationDirection::backward)) {
                return false;
            }
            this->navigationHistory.pop_back();
            return true;
        }
        const auto route = std::find_if(Routes().begin(), Routes().end(), [this, pageName](const NavigationRoute& candidate) {
            return candidate.targetKind == NavigationTargetKind::page
                && candidate.source == this->currentPage->Name()
                && candidate.target == pageName;
        });
        const auto direction = route == Routes().end()
            ? mobileclock::presentation::core::NavigationDirection::forward
            : route->direction;
        if (!this->SwitchPage(*page, direction)) {
            return false;
        }
        this->navigationHistory.push_back(page);
        return true;
    }

    bool PageManager::NavigateBack(std::unique_ptr<base::NavigationStateBase> result) {
        if (this->currentPage == nullptr || this->isTransitioning) {
            return false;
        }
        const auto route = std::find_if(Routes().begin(), Routes().end(), [this](const NavigationRoute& candidate) {
            return candidate.source == this->currentPage->Name()
                && candidate.trigger == NavigationTrigger::navigateBack;
        });
        if (route == Routes().end() || this->ResolveTarget(*route).empty()) {
            return false;
        }
        // Страница может подготовить состояние при уходе. Явный result имеет приоритет, чтобы
        // действие «Применить» могло передать данные, не создавая отдельный обратный маршрут.
        const NavigationRequest request{route->source, this->ResolveTarget(*route), route->trigger};
        std::unique_ptr<base::NavigationStateBase> state = this->currentPage->OnNavigatingFrom(request);
        if (result != nullptr) {
            state = std::move(result);
        }
        return this->Navigate(*route, std::move(state));
    }

    bool PageManager::Trigger(NavigationTrigger trigger) {
        if (trigger == NavigationTrigger::navigateBack) {
            return this->NavigateBack();
        }
        if (this->currentPage == nullptr || this->isTransitioning) {
            return false;
        }
        const auto route = std::find_if(Routes().begin(), Routes().end(), [this, trigger](const NavigationRoute& candidate) {
            return candidate.source == this->currentPage->Name() && candidate.trigger == trigger;
        });
        if (route == Routes().end()) {
            LOG_WARNING(
                "MobileClock.Navigation",
                "No route for page '{}' and trigger '{}'",
                this->currentPage->Name(),
                static_cast<int>(trigger));
            return false;
        }
        const std::string_view targetName = this->ResolveTarget(*route);
        if (targetName.empty()) {
            LOG_WARNING("MobileClock.Navigation", "Route '{}' has no current target", route->id);
            return false;
        }
        const NavigationRequest request{route->source, targetName, route->trigger};
        return this->Navigate(*route, this->currentPage->OnNavigatingFrom(request));
    }

    std::string_view PageManager::ResolveTarget(const NavigationRoute& route) const {
        if (route.targetKind == NavigationTargetKind::page) {
            return route.target;
        }
        if (this->navigationHistory.size() < 2) {
            return {};
        }
        // Последняя запись — текущая страница, предпоследняя — единственная актуальная цель
        // возврата, даже если в статическом графе к текущей странице ведёт несколько путей.
        return this->navigationHistory[this->navigationHistory.size() - 2]->Name();
    }

    bool PageManager::Navigate(const NavigationRoute& route, std::unique_ptr<base::NavigationStateBase> state) {
        const std::string_view targetName = this->ResolveTarget(route);
        interface::IPage* const target = this->pages.Find(targetName);
        if (target == nullptr) {
            LOG_WARNING("MobileClock.Navigation", "Route target '{}' is not registered", targetName);
            return false;
        }
        const NavigationRequest request{route.source, targetName, route.trigger};
        if (!target->OnNavigatingTo(request, std::move(state))) {
            LOG_WARNING("MobileClock.Navigation", "Route preparation failed for {} -> {}", route.source, targetName);
            return false;
        }
        if (!this->SwitchPage(*target, route.direction)) {
            return false;
        }
        // Историю меняем только после успешной подготовки target и переключения визуального
        // состояния: отклонённая навигация не должна менять будущий маршрут возврата.
        if (route.targetKind == NavigationTargetKind::previousPage) {
            this->navigationHistory.pop_back();
        } else {
            this->navigationHistory.push_back(target);
        }
        return true;
    }

    bool PageManager::SwitchPage(interface::IPage& page, mobileclock::presentation::core::NavigationDirection direction) {
        if (this->currentPage == &page || this->isTransitioning) {
            return this->currentPage == &page;
        }
        this->outgoingPage = this->currentPage;
        this->currentPage = &page;
        this->navigationDirection = direction;
        this->pages.ForEach([&page](interface::IPage& candidate) {
            candidate.Root().SetVisibility(&candidate == &page
                ? xaml::attr::Visibility::visible
                : xaml::attr::Visibility::collapsed);
        });
        SetNavigationVisualStates(this->outgoingPage, *this->currentPage, direction);
        this->isTransitioning = this->pages.IsAnyAnimating();
        return true;
    }
    std::string_view PageManager::CurrentPageName() const {
        return this->currentPage == nullptr ? std::string_view{} : this->currentPage->Name();
    }

    bool PageManager::IsTransitioning() const {
        return this->isTransitioning;
    }

    //
    // API
    //
    void PageManager::Initialize(xaml::Size availableSize) {
        this->inputDispatcher.Cancel();
        this->animations = xaml::AnimationController{};
        this->availableSize = availableSize;
        xaml::AnimationRegistry registry;
        mobileclock::presentation::core::RegisterAnimations(registry);
        const auto parameters = [this]() {
            return xaml::AnimationParameters::Create(mobileclock::presentation::core::PageTransitionData{
                this->outgoingPage == nullptr ? "" : std::string(this->outgoingPage->Name()),
                this->currentPage == nullptr ? "" : std::string(this->currentPage->Name()),
                this->navigationDirection,
            });
        };
        this->pages.ForEach([&](interface::IPage& page) {
            page.Initialize(this->availableSize);
            page.Root().SetAnimationParametersProvider(parameters);
            page.Root().SetVisibility(&page == &this->pages.GetPage<ui::page::MainPageViewModel>()
                ? xaml::attr::Visibility::visible
                : xaml::attr::Visibility::collapsed);
            this->animations.Attach(page.Root(), registry);
        });
        this->currentPage = &this->pages.GetPage<ui::page::MainPageViewModel>();
        this->outgoingPage = this->currentPage;
        // Новая сессия всегда начинает новый стек, иначе возврат мог бы попасть в страницу
        // предыдущей сессии previewer-а.
        this->navigationHistory.clear();
        this->navigationHistory.push_back(this->currentPage);
        this->currentPage->Root().SetVisibility(xaml::attr::Visibility::visible);
        this->isTransitioning = false;
    }

    void PageManager::Resize(xaml::Size availableSize) {
        this->availableSize = availableSize;
        this->pages.ForEach([availableSize](interface::IPage& page) {
            xaml::layout(page.Root(), availableSize);
        });
    }

    void PageManager::SetAnimationPlaybackRate(float value) {
        this->animations.SetPlaybackRate(value);
    }

    void PageManager::SetStatus(std::string value) {
        this->pages.Get<ui::page::MainPageViewModel>().SetStatus(std::move(value));
    }

    void PageManager::AddAlarmMelody(model::AlarmMelody alarmMelody) {
        this->pageContext.alarmMelodyRepository.SaveMelody(alarmMelody);
    }

    void PageManager::SetAlarmMelody(model::AlarmMelody alarmMelody) {
        ui::page::AddAlarmPageViewModel& page = this->pages.Get<ui::page::AddAlarmPageViewModel>();
        page.SetMelody(std::move(alarmMelody));
    }

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    bool PageManager::NavigatePreviewRoute(std::string_view target, std::string& error) {
        error.clear();
        if (this->currentPage == nullptr) {
            error = "Preview route requires an initialized application session";
            LOG_WARNING("MobileClock.PreviewRoute", "Route request to '{}' rejected: {}", target, error);
            return false;
        }
        LOG_INFO(
            "MobileClock.PreviewRoute",
            "Route request: current='{}', target='{}'",
            this->currentPage->Name(),
            target);
        if (this->currentPage->Name() == target) {
            LOG_INFO("MobileClock.PreviewRoute", "Route request ignored because '{}' is already active", target);
            return true;
        }
        struct RouteNode final {
            std::string_view page;
            size_t previousNode;
            const NavigationRoute* incomingRoute;
        };
        std::vector<RouteNode> nodes{{this->currentPage->Name(), 0, nullptr}};
        constexpr size_t noRoute = std::numeric_limits<size_t>::max();
        size_t targetNode = noRoute;
        for (size_t index = 0; index < nodes.size() && targetNode == noRoute; ++index) {
            for (const NavigationRoute& route : Routes()) {
                if (route.source != nodes[index].page) {
                    continue;
                }
                if (route.targetKind == NavigationTargetKind::previousPage) {
                    continue;
                }
                const bool wasVisited = std::any_of(nodes.begin(), nodes.end(), [&route](const RouteNode& node) {
                    return node.page == route.target;
                });
                if (wasVisited) {
                    continue;
                }
                nodes.push_back({route.target, index, &route});
                if (route.target == target) {
                    targetNode = nodes.size() - 1;
                    break;
                }
            }
        }
        if (targetNode == noRoute) {
            error = std::format("No preview route from {} to {}", this->currentPage->Name(), target);
            LOG_WARNING("MobileClock.PreviewRoute", "Route request rejected: {}", error);
            return false;
        }
        std::vector<const NavigationRoute*> route;
        for (size_t index = targetNode; index != 0; index = nodes[index].previousNode) {
            route.push_back(nodes[index].incomingRoute);
        }
        std::reverse(route.begin(), route.end());
        return this->ExecutePreviewRoute(route, error);
    }

    bool PageManager::NavigatePreviewTransitions(std::span<const std::string_view> transitionIds, std::string& error) {
        error.clear();
        if (this->currentPage == nullptr || transitionIds.empty()) {
            error = "Preview route requires an initialized application session and transition IDs";
            LOG_WARNING("MobileClock.PreviewRoute", "Explicit route rejected: {}", error);
            return false;
        }
        std::string serializedTransitionIds;
        std::vector<const NavigationRoute*> route;
        route.reserve(transitionIds.size());
        for (const std::string_view id : transitionIds) {
            if (!serializedTransitionIds.empty()) {
                serializedTransitionIds += '>';
            }
            serializedTransitionIds += id;
        }
        LOG_INFO(
            "MobileClock.PreviewRoute",
            "Explicit route request: current='{}', transitionIds='{}'",
            this->currentPage->Name(),
            serializedTransitionIds);
        // PreviewRoutes разворачивает весь текущий стек previousPage в конкретные рёбра.
        // Это позволяет проверить Xiaomi Themes → Новый будильник → Главная до начала переходов.
        const std::vector<PreviewRoute> previewRoutes = this->PreviewRoutes();
        std::string_view expectedSource = this->currentPage->Name();
        for (const std::string_view id : transitionIds) {
            const auto previewEdge = std::find_if(previewRoutes.begin(), previewRoutes.end(), [id](const PreviewRoute& candidate) {
                return candidate.id == id;
            });
            if (previewEdge == previewRoutes.end()) {
                error = std::format("Unknown preview transition '{}'", id);
                LOG_WARNING("MobileClock.PreviewRoute", "Explicit route rejected: {}", error);
                return false;
            }
            const auto edge = std::find_if(Routes().begin(), Routes().end(), [id](const NavigationRoute& candidate) {
                return candidate.id == id;
            });
            if (edge == Routes().end()) {
                error = std::format("Unknown preview transition '{}'", id);
                LOG_WARNING("MobileClock.PreviewRoute", "Explicit route rejected: {}", error);
                return false;
            }
            if (previewEdge->source != expectedSource) {
                error = std::format(
                    "Preview transition '{}' starts at {}, but the active route page is {}",
                    id,
                    previewEdge->source,
                    expectedSource);
                LOG_WARNING("MobileClock.PreviewRoute", "Explicit route rejected: {}", error);
                return false;
            }
            route.push_back(&*edge);
            expectedSource = previewEdge->target;
        }
        return this->ExecutePreviewRoute(route, error);
    }

    bool PageManager::NavigatePreviewRoute(std::span<const std::string_view> path, std::string& error) {
        error.clear();
        if (this->currentPage == nullptr || path.empty()) {
            error = "Preview route requires an initialized application session and a path";
            LOG_WARNING("MobileClock.PreviewRoute", "Explicit route rejected: {}", error);
            return false;
        }
        if (path.front() != this->currentPage->Name()) {
            error = std::format("Preview route starts at {}, but the active page is {}", path.front(), this->currentPage->Name());
            LOG_WARNING("MobileClock.PreviewRoute", "Explicit route rejected: {}", error);
            return false;
        }
        std::vector<const NavigationRoute*> route;
        const std::vector<PreviewRoute> previewRoutes = this->PreviewRoutes();
        for (size_t index = 1; index < path.size(); ++index) {
            const auto previewEdge = std::find_if(previewRoutes.begin(), previewRoutes.end(), [&](const PreviewRoute& candidate) {
                return candidate.source == path[index - 1] && candidate.target == path[index];
            });
            if (previewEdge == previewRoutes.end()) {
                error = std::format("No preview transition from {} to {}", path[index - 1], path[index]);
                LOG_WARNING("MobileClock.PreviewRoute", "Explicit route rejected: {}", error);
                return false;
            }
            const auto edge = std::find_if(Routes().begin(), Routes().end(), [previewEdge](const NavigationRoute& candidate) {
                return candidate.id == previewEdge->id;
            });
            route.push_back(&*edge);
        }
        return this->ExecutePreviewRoute(route, error);
    }

    std::string PageManager::PreviewRouteGraph() const {
        std::string result;
        for (const NavigationRoute& route : Routes()) {
            std::string_view target;
            if (route.targetKind == NavigationTargetKind::previousPage) {
                // В preview-графе показываем весь стек возврата. При реальном переходе
                // active page всё равно снимается только один верхний элемент истории.
                for (size_t index = this->navigationHistory.size(); index > 1; --index) {
                    if (this->navigationHistory[index - 1]->Name() == route.source) {
                        target = this->navigationHistory[index - 2]->Name();
                        break;
                    }
                }
            } else {
                target = route.target;
            }
            if (target.empty()) {
                continue;
            }
            if (!result.empty()) {
                result += ';';
            }
            result += route.source;
            result += '>';
            result += target;
        }
        return result;
    }

    std::vector<PageManager::PreviewRoute> PageManager::PreviewRoutes() const {
        std::vector<PreviewRoute> result;
        result.reserve(Routes().size());
        for (const NavigationRoute& route : Routes()) {
            std::string_view target;
            if (route.targetKind == NavigationTargetKind::previousPage) {
                // Статическое ребро «назад» становится конкретным ребром для каждого
                // узла текущей истории: Xiaomi Themes → Новый будильник → Главная.
                for (size_t index = this->navigationHistory.size(); index > 1; --index) {
                    if (this->navigationHistory[index - 1]->Name() == route.source) {
                        target = this->navigationHistory[index - 2]->Name();
                        break;
                    }
                }
            } else {
                target = route.target;
            }
            if (!target.empty()) {
                result.push_back({route.id, route.source, target, route.title, route.isDefault, route.targetKind});
            }
        }
        return result;
    }

    std::string_view PageManager::PreviewPageTitle(std::string_view pageName) const {
        const interface::IPage* const page = this->pages.Find(pageName);
        return page == nullptr ? pageName : page->PreviewGraphTitle();
    }

    bool PageManager::ApplyPreviewScenario(std::string_view pageName, std::string_view json, std::string& error) {
        interface::IPage* const page = this->pages.Find(pageName);
        if (page == nullptr) {
            error = "Unknown MobileClock page";
            return false;
        }
        return page->ApplyScenario(json, error);
    }

    bool PageManager::ReloadMarkup(std::string_view pageName, std::string_view markup,
        std::string_view sourcePath, std::string& diagnostics) {
        diagnostics.clear();
        try {
            interface::IPage* const page = this->pages.Find(pageName);
            if (page == nullptr) {
                throw std::invalid_argument("Unknown MobileClock page");
            }
            // Сначала разбираем разметку. До успешного завершения всей подготовки
            // отображаемое дерево страницы не изменяется.
            const auto ast = xaml::runtime::XamlParser{}.Parse(markup, sourcePath);
            if (ast.name == "UserControl") {
                if (ast.children.size() != 1) {
                    throw std::invalid_argument("UserControl requires exactly one content root");
                }
                std::string className;
                for (const auto& attribute : ast.attributes) {
                    if (attribute.name == "Class" && attribute.nameSpace == "http://schemas.microsoft.com/winfx/2006/xaml") {
                        className = attribute.value;
                    }
                }
                // UserControl описывает шаблон уже существующего native-контрола.
                // Ищем его в текущей странице, чтобы не потерять его состояние и обработчики ввода.
                const auto find = [&className](auto&& self, xaml::Element& element) -> xaml::runtime::IRuntimeReloadableControl* {
                    if (auto* control = dynamic_cast<xaml::runtime::IRuntimeReloadableControl*>(&element)) {
                        if (control->RuntimeClassName() == className) {
                            return control;
                        }
                    }
                    for (const auto& child : element.Children()) {
                        if (auto* found = self(self, *child)) {
                            return found;
                        }
                    }
                    return nullptr;
                };
                auto* control = find(find, page->Root());
                if (control == nullptr) {
                    throw std::invalid_argument("No reloadable native control on this page");
                }
                auto context = page->RuntimeContext();
                // Смещение прокрутки восстанавливается после замены шаблона: новый
                // ScrollViewer не наследует состояние прежнего экземпляра.
                const auto pan = this->inputDispatcher.CaptureRuntimePan();
                context.prepareTree = [](xaml::Element& root) {
                    xaml::AnimationRegistry registry;
                    mobileclock::presentation::core::RegisterAnimations(registry);
                    registry.ValidateTree(root);
                    xaml::AnimationController validation;
                    validation.Attach(root, registry);
                };
                context.beforeCommit = [this]() { this->inputDispatcher.Cancel(); };
                if (!control->ReplaceTemplate(ast, context, diagnostics)) {
                    return false;
                }
                this->inputDispatcher.RestoreRuntimePan(page->Root(), pan);
                xaml::layout(page->Root(), this->availableSize);
                return true;
            }
            auto context = page->RuntimeContext();
            // Prepare строит и проверяет отдельное дерево. Если парсинг, биндинг или
            // валидация завершаются ошибкой, текущая страница остаётся без изменений.
            auto result = xaml::runtime::RuntimeReloadTransaction{}.Prepare(
                markup, sourcePath, context, page->Root(), this->availableSize);
            xaml::AnimationRegistry registry;
            mobileclock::presentation::core::RegisterAnimations(registry);
            result.root->SetAnimationParametersProvider([this]() {
                return xaml::AnimationParameters::Create(mobileclock::presentation::core::PageTransitionData{
                    this->outgoingPage == nullptr ? "" : std::string(this->outgoingPage->Name()),
                    this->currentPage == nullptr ? "" : std::string(this->currentPage->Name()),
                    this->navigationDirection});
            });
            this->animations.Attach(*result.root, registry);
            registry.ValidateTree(*result.root);
            if (context.prepareTree) {
                context.prepareTree(*result.root);
            }
            xaml::layout(*result.root, this->availableSize);
            // Отменяем активный жест до передачи владения новым деревом, поскольку
            // InputDispatcher мог хранить указатель на элемент старого дерева.
            const auto pan = this->inputDispatcher.CaptureRuntimePan();
            this->inputDispatcher.Cancel();
            // Это единственная точка, где полная перезагрузка страницы становится видимой.
            page->ReplaceRuntimeTree(std::move(result));
            this->inputDispatcher.RestoreRuntimePan(page->Root(), pan);
            this->isTransitioning = false;
            return true;
        } catch (const std::exception& error) {
            diagnostics = error.what();
            if (diagnostics.find(std::string(sourcePath) + ":") != 0) {
                diagnostics = std::string(sourcePath) + ":1:1: " + diagnostics;
            }
            return false;
        }
    }
#endif

    void PageManager::HandleTouchDown(float x, float y) {
        if (this->isTransitioning) {
            return;
        }
        this->inputDispatcher.PointerDown(this->currentPage->Root(), x, y, &this->animations);
    }

    bool PageManager::HandleTouchMove(float x, float y) {
        if (this->isTransitioning) {
            return false;
        }
        return this->inputDispatcher.PointerMove(x, y);
    }

    bool PageManager::HandleTouchUp(float x, float y) {
        if (this->isTransitioning) {
            return false;
        }
        xaml::Element* const element = this->inputDispatcher.PointerUp(
            this->currentPage->Root(),
            x,
            y,
            this->animations);
        if (element == nullptr) {
            return false;
        }
        this->currentPage->HandleTap(*element);
        return true;
    }

    void PageManager::CancelTouch() {
        this->inputDispatcher.Cancel();
    }

    xaml::Element& PageManager::Root() {
        return this->currentPage->Root();
    }

    void PageManager::UpdateClock() {
        this->animations.Update();
        this->inputDispatcher.Update(this->currentPage->Root(), this->animations);
        if (this->isTransitioning && !this->pages.IsAnyAnimating()) {
            this->isTransitioning = false;
        }
        this->pages.ForEach([](interface::IPage& page) {
            page.Update();
        });
    }

    void PageManager::Render(
        xaml::IRenderBackend& renderer,
        const xaml::RendererRegistry& renderers) const {
        if (this->isTransitioning) {
            this->outgoingPage->Render(renderer, renderers);
        }
        this->currentPage->Render(renderer, renderers);
    }

    //
    // Internal
    //
    template <
        typename TSource,
        typename TTarget,
        NavigationTrigger TTrigger,
        mobileclock::presentation::core::NavigationDirection TDirection
    >
    PageManager::NavigationRoute PageManager::MakeRoute(std::string_view id, std::string_view title, bool isDefault) {
        return {
            id,
            TSource::PageName,
            TTrigger,
            TTarget::PageName,
            NavigationTargetKind::page,
            TDirection,
            title,
            isDefault,
        };
    }

    template <
        typename TSource,
        NavigationTrigger TTrigger,
        mobileclock::presentation::core::NavigationDirection TDirection
    >
    PageManager::NavigationRoute PageManager::MakeBackRoute(std::string_view id, std::string_view title, bool isDefault) {
        return {
            id,
            TSource::PageName,
            TTrigger,
            {},
            NavigationTargetKind::previousPage,
            TDirection,
            title,
            isDefault,
        };
    }

    void PageManager::SetNavigationVisualStates(
        interface::IPage* outgoing,
        interface::IPage& current,
        mobileclock::presentation::core::NavigationDirection direction) {
        const std::string_view state = direction == mobileclock::presentation::core::NavigationDirection::forward
            ? "Forward" : "Backward";
        const auto set = [state](interface::IPage& page) {
            xaml::VisualStateManager::GoToState(page.Root(), "NavigationDirection", "Idle", false);
            xaml::VisualStateManager::GoToState(page.Root(), "NavigationDirection", std::string(state));
        };
        if (outgoing != nullptr) {
            set(*outgoing);
        }
        set(current);
    }

    std::span<const PageManager::NavigationRoute> PageManager::Routes() {
        static const std::array routes{
            MakeRoute<ui::page::MainPageViewModel, ui::page::AddAlarmPageViewModel, NavigationTrigger::createAlarm, mobileclock::presentation::core::NavigationDirection::forward>("main-create-alarm", "Новый будильник"),
            MakeRoute<ui::page::MainPageViewModel, ui::page::AddAlarmPageViewModel, NavigationTrigger::editAlarm, mobileclock::presentation::core::NavigationDirection::forward>("main-edit-alarm", "Изменить будильник", false),
            MakeRoute<ui::page::MainPageViewModel, ui::page::SettingsPageViewModel, NavigationTrigger::navigateToSettings, mobileclock::presentation::core::NavigationDirection::forward>("main-open-settings", "Открыть настройки"),
            MakeBackRoute<ui::page::AddAlarmPageViewModel, NavigationTrigger::navigateBack, mobileclock::presentation::core::NavigationDirection::backward>("add-alarm-back", "Вернуться назад"),
#if defined(MOBILECLOCK_XAML_PREVIEWER)
            MakeRoute<ui::page::AddAlarmPageViewModel, ui::page::XiaomiThemesPageViewModel, NavigationTrigger::chooseAlarmMelody, mobileclock::presentation::core::NavigationDirection::forward>("add-alarm-choose-melody", "Выбрать мелодию"),
            MakeBackRoute<ui::page::XiaomiThemesPageViewModel, NavigationTrigger::navigateBack, mobileclock::presentation::core::NavigationDirection::backward>("xiaomi-themes-back", "Вернуться назад"),
#endif
            MakeBackRoute<ui::page::SettingsPageViewModel, NavigationTrigger::navigateBack, mobileclock::presentation::core::NavigationDirection::backward>("settings-back", "Вернуться назад"),
        };
        return routes;
    }

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    bool PageManager::ExecutePreviewRoute(std::span<const NavigationRoute*> route, std::string& error) {
        for (const NavigationRoute* edge : route) {
            if (edge == nullptr) {
                error = "Preview route contains an invalid transition";
                LOG_ERROR("MobileClock.PreviewRoute", "Route execution failed: {}", error);
                return false;
            }
            const std::string_view previousPage = this->CurrentPageName();
            const std::string_view expectedTarget = this->ResolveTarget(*edge);
            if (expectedTarget.empty()) {
                error = std::format("Preview transition '{}' has no current target", edge->id);
                LOG_ERROR("MobileClock.PreviewRoute", "Route execution failed: {}", error);
                return false;
            }
            LOG_INFO(
                "MobileClock.PreviewRoute",
                "Executing transition '{}': {} -> {}",
                edge->id,
                edge->source,
                expectedTarget);
            this->isTransitioning = false;
            bool navigated = false;
#if MOBILECLOCK_ENABLE_PREVIEW_EDIT_ALARM_FALLBACK
            if (edge->trigger == NavigationTrigger::editAlarm) {
                const NavigationRequest request{edge->source, expectedTarget, edge->trigger};
                std::unique_ptr<base::NavigationStateBase> state = this->currentPage->OnNavigatingFrom(request);
                if (state == nullptr) {
                    // Граф не выбирает строку конкретного будильника, но должен позволять
                    // проверить переход редактирования. Берём первый сценарный будильник,
                    // а при пустом сценарии создаём временное корректное состояние.
                    const std::vector<model::Alarm>& alarms = this->pageContext.alarmRepository.Alarms();
                    const model::Alarm previewAlarm = alarms.empty() ? model::Alarm{} : alarms.front();
                    const std::string alarmId = previewAlarm.id.empty() ? "preview-alarm" : previewAlarm.id;
                    state = std::make_unique<AlarmEditNavigationState>(alarmId, previewAlarm);
                }
                navigated = this->Navigate(*edge, std::move(state));
            } else {
#endif
                navigated = this->Trigger(edge->trigger);
#if MOBILECLOCK_ENABLE_PREVIEW_EDIT_ALARM_FALLBACK
            }
#endif
            if (!navigated) {
                error = std::format("Preview transition {} -> {} was rejected", edge->source, expectedTarget);
                LOG_ERROR("MobileClock.PreviewRoute", "Route execution failed: {}", error);
                return false;
            }
            const std::string_view currentPage = this->CurrentPageName();
            if (currentPage != expectedTarget) {
                error = std::format(
                    "Preview transition {} -> {} ended on {}",
                    edge->source,
                    expectedTarget,
                    currentPage);
                LOG_ERROR(
                    "MobileClock.PreviewRoute",
                    "Route execution failed after '{}': expected='{}', actual='{}', previous='{}'",
                    error,
                    expectedTarget,
                    currentPage,
                    previousPage);
                return false;
            }
            LOG_INFO("MobileClock.PreviewRoute", "Transition completed: active='{}'", currentPage);
        }
        return true;
    }
#endif
}