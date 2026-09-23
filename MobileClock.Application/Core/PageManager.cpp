#include "PageManager.h"

#include <Helpers.Logging/Logging.h>
#if defined(ANDROID_APP_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/IRuntimeReloadableControl.h>
#include <XamlRuntime/RuntimeMarkup/RuntimeReloadTransaction.h>
#include <XamlRuntime/RuntimeMarkup/XamlParser.h>
#endif
#include <XamlRuntime/RenderEngine.h>

#include "MobileClock.Presentation/Renderer/AnimationRenderers.h"
#include "MobileClock.Presentation/Core/PageTransition.h"
#include "MobileClock.Presentation/Core/Registrations.h"

#include "NavigationStates.h"

#include <type_traits>
#include <algorithm>
#include <format>
#include <limits>
#include <vector>
#include <array>

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
        if (this->navigationHistory.size() > 1 && this->navigationHistory[this->navigationHistory.size() - 2].page == page) {
            if (!this->SwitchPage(*page, mobileclock::presentation::core::NavigationDirection::backward)) {
                return false;
            }
            this->navigationHistory.pop_back();
            return true;
        }
        const auto navigationRoute = std::find_if(Routes().begin(), Routes().end(), [this, pageName](const NavigationRoute& candidate) {
            return candidate.targetKind == NavigationTargetKind::page
                && candidate.source == this->currentPage->Name()
                && candidate.target == pageName;
        });
        const auto direction = navigationRoute == Routes().end()
            ? mobileclock::presentation::core::NavigationDirection::forward
            : navigationRoute->direction;
        if (!this->SwitchPage(*page, direction)) {
            return false;
        }
        this->navigationHistory.push_back({page, navigationRoute == Routes().end() ? nullptr : &*navigationRoute});
        return true;
    }

    bool PageManager::NavigateBack(std::unique_ptr<base::NavigationStateBase> result) {
        if (this->currentPage == nullptr || this->isTransitioning) {
            return false;
        }
        const auto navigationRoute = std::find_if(Routes().begin(), Routes().end(), [this](const NavigationRoute& candidate) {
            return candidate.source == this->currentPage->Name()
                && candidate.trigger == NavigationTrigger::navigateBack;
        });
        if (navigationRoute == Routes().end() || this->ResolveTarget(*navigationRoute).empty()) {
            return false;
        }
        // Страница может подготовить состояние при уходе. Явный result имеет приоритет, чтобы
        // действие «Применить» могло передать данные, не создавая отдельный обратный маршрут.
        const NavigationRequest request{navigationRoute->source, this->ResolveTarget(*navigationRoute), navigationRoute->trigger};
        std::unique_ptr<base::NavigationStateBase> state = this->currentPage->OnNavigatingFrom(request);
        if (result != nullptr) {
            state = std::move(result);
        }
        return this->Navigate(*navigationRoute, std::move(state));
    }

    bool PageManager::Trigger(NavigationTrigger trigger) {
        return this->Trigger(trigger, {});
    }

    bool PageManager::Trigger(NavigationTrigger trigger, std::unique_ptr<base::NavigationStateBase> state) {
        if (trigger == NavigationTrigger::navigateBack) {
            return this->NavigateBack(std::move(state));
        }
        if (this->currentPage == nullptr || this->isTransitioning) {
            return false;
        }
        const auto navigationRoute = std::find_if(Routes().begin(), Routes().end(), [this, trigger](const NavigationRoute& candidate) {
            return candidate.source == this->currentPage->Name() && candidate.trigger == trigger;
        });
        if (navigationRoute == Routes().end()) {
            LOG_WARNING(
                "MobileClock.Navigation",
                "No route for page '{}' and trigger '{}'",
                this->currentPage->Name(),
                static_cast<int>(trigger));
            return false;
        }
        const std::string_view targetName = this->ResolveTarget(*navigationRoute);
        if (targetName.empty()) {
            LOG_WARNING("MobileClock.Navigation", "Route '{}' has no current target", navigationRoute->id);
            return false;
        }
        const NavigationRequest request{navigationRoute->source, targetName, navigationRoute->trigger};
        // Явный payload создаётся действием пользователя и имеет приоритет над устаревшим
        // OnNavigatingFrom. Это убирает скрытую зависимость маршрута от состояния страницы.
        if (state == nullptr) {
            state = this->currentPage->OnNavigatingFrom(request);
        }
        return this->Navigate(*navigationRoute, std::move(state));
    }

    std::string_view PageManager::ResolveTarget(const NavigationRoute& navigationRoute) const {
        if (navigationRoute.targetKind == NavigationTargetKind::page) {
            return navigationRoute.target;
        }
        if (this->navigationHistory.size() < 2) {
            return {};
        }
        // Последняя запись — текущая страница, предпоследняя — единственная актуальная цель
        // возврата, даже если в статическом графе к текущей странице ведёт несколько путей.
        return this->navigationHistory[this->navigationHistory.size() - 2].page->Name();
    }

    bool PageManager::Navigate(const NavigationRoute& navigationRoute, std::unique_ptr<base::NavigationStateBase> state) {
        const std::string_view targetName = this->ResolveTarget(navigationRoute);
        interface::IPage* const target = this->pages.Find(targetName);
        if (target == nullptr) {
            LOG_WARNING("MobileClock.Navigation", "Route target '{}' is not registered", targetName);
            return false;
        }
        if (!this->IsNavigationDataValid(navigationRoute, state.get())) {
            LOG_WARNING("MobileClock.Navigation", "Route '{}' received invalid navigation data", navigationRoute.id);
            return false;
        }
        const NavigationRequest request{navigationRoute.source, targetName, navigationRoute.trigger};
        if (!target->OnNavigatingTo(request, std::move(state))) {
            LOG_WARNING("MobileClock.Navigation", "Route preparation failed for {} -> {}", navigationRoute.source, targetName);
            return false;
        }
        if (!this->SwitchPage(*target, navigationRoute.direction)) {
            return false;
        }
        // Историю меняем только после успешной подготовки target и переключения визуального
        // состояния: отклонённая навигация не должна менять будущий маршрут возврата.
        if (navigationRoute.targetKind == NavigationTargetKind::previousPage) {
            this->navigationHistory.pop_back();
        } else {
            this->navigationHistory.push_back({target, &navigationRoute});
        }
        return true;
    }

    bool PageManager::IsNavigationDataValid(const NavigationRoute& navigationRoute, const base::NavigationStateBase* state) const {
        if (navigationRoute.dataContract == nullptr) {
            return state == nullptr;
        }
        if (state == nullptr) {
            return !navigationRoute.dataContract->isRequired;
        }
        return state->TypeId() == navigationRoute.dataContract->typeId;
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
        this->animationController = xaml::AnimationController{};
        this->availableSize = availableSize;
        xaml::AnimationRegistry animationRegistry;
        mobileclock::presentation::core::RegisterAnimations(animationRegistry);
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
            this->animationController.Attach(page.Root(), animationRegistry);
        });
        this->currentPage = &this->pages.GetPage<ui::page::MainPageViewModel>();
        this->outgoingPage = this->currentPage;
        // Новая сессия всегда начинает новый стек, иначе возврат мог бы попасть в страницу
        // предыдущей сессии previewer-а.
        this->navigationHistory.clear();
        this->navigationHistory.push_back({this->currentPage, nullptr});
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
        this->animationController.SetPlaybackRate(value);
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

#if defined(ANDROID_APP_PREVIEWER)
    bool PageManager::preview_NavigateRoute(std::string_view target, std::string& error) {
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
        struct preview_RouteNode final {
            std::string_view page;
            size_t previousNode;
            const NavigationRoute* incomingRoute;
        };
        std::vector<preview_RouteNode> nodes{{this->currentPage->Name(), 0, nullptr}};
        constexpr size_t noRoute = std::numeric_limits<size_t>::max();
        size_t targetNode = noRoute;
        for (size_t index = 0; index < nodes.size() && targetNode == noRoute; ++index) {
            for (const NavigationRoute& navigationRoute : Routes()) {
                if (navigationRoute.source != nodes[index].page) {
                    continue;
                }
                if (navigationRoute.targetKind == NavigationTargetKind::previousPage) {
                    continue;
                }
                const bool wasVisited = std::any_of(nodes.begin(), nodes.end(), [&navigationRoute](const preview_RouteNode& node) {
                    return node.page == navigationRoute.target;
                });
                if (wasVisited) {
                    continue;
                }
                nodes.push_back({navigationRoute.target, index, &navigationRoute});
                if (navigationRoute.target == target) {
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
        std::vector<const NavigationRoute*> navigationRoutes;
        for (size_t index = targetNode; index != 0; index = nodes[index].previousNode) {
            navigationRoutes.push_back(nodes[index].incomingRoute);
        }
        std::reverse(navigationRoutes.begin(), navigationRoutes.end());
        return this->preview_ExecuteRoute(navigationRoutes, error);
    }

    bool PageManager::preview_NavigateTransitions(std::span<const std::string_view> transitionIds, std::string& error) {
        error.clear();
        if (this->currentPage == nullptr || transitionIds.empty()) {
            error = "Preview route requires an initialized application session and transition IDs";
            LOG_WARNING("MobileClock.PreviewRoute", "Explicit route rejected: {}", error);
            return false;
        }
        std::string serializedTransitionIds;
        std::vector<const NavigationRoute*> navigationRoutes;
        navigationRoutes.reserve(transitionIds.size());
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
        // preview_Routes разворачивает весь текущий стек previousPage в конкретные рёбра.
        // Это позволяет проверить Xiaomi Themes → Новый будильник → Главная до начала переходов.
        const std::vector<preview_Route> previewRoutes = this->preview_Routes();
        std::string_view expectedSource = this->currentPage->Name();
        for (const std::string_view id : transitionIds) {
            const auto previewEdge = std::find_if(previewRoutes.begin(), previewRoutes.end(), [id](const preview_Route& candidate) {
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
            navigationRoutes.push_back(&*edge);
            expectedSource = previewEdge->target;
        }
        return this->preview_ExecuteRoute(navigationRoutes, error);
    }

    bool PageManager::preview_NavigateRoute(std::span<const std::string_view> path, std::string& error) {
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
        std::vector<const NavigationRoute*> navigationRoutes;
        const std::vector<preview_Route> previewRoutes = this->preview_Routes();
        for (size_t index = 1; index < path.size(); ++index) {
            const auto previewEdge = std::find_if(previewRoutes.begin(), previewRoutes.end(), [&](const preview_Route& candidate) {
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
            navigationRoutes.push_back(&*edge);
        }
        return this->preview_ExecuteRoute(navigationRoutes, error);
    }

    std::string PageManager::preview_RouteGraph() const {
        std::string result;
        for (const NavigationRoute& navigationRoute : Routes()) {
            std::string_view target;
            if (navigationRoute.targetKind == NavigationTargetKind::previousPage) {
                // В preview-графе показываем весь стек возврата. При реальном переходе
                // active page всё равно снимается только один верхний элемент истории.
                for (size_t index = this->navigationHistory.size(); index > 1; --index) {
                    if (this->navigationHistory[index - 1].page->Name() == navigationRoute.source) {
                        target = this->navigationHistory[index - 2].page->Name();
                        break;
                    }
                }
            } else {
                target = navigationRoute.target;
            }
            if (target.empty()) {
                continue;
            }
            if (!result.empty()) {
                result += ';';
            }
            result += navigationRoute.source;
            result += '>';
            result += target;
        }
        return result;
    }

    std::vector<PageManager::preview_Route> PageManager::preview_Routes() const {
        std::vector<preview_Route> result;
        result.reserve(Routes().size());
        for (const NavigationRoute& navigationRoute : Routes()) {
            std::string_view target;
            std::string_view backwardOfRouteId;
            if (navigationRoute.targetKind == NavigationTargetKind::previousPage) {
                // Статическое ребро «назад» становится конкретным ребром для каждого
                // узла текущей истории: Xiaomi Themes → Новый будильник → Главная.
                for (size_t index = this->navigationHistory.size(); index > 1; --index) {
                    if (this->navigationHistory[index - 1].page->Name() == navigationRoute.source) {
                        target = this->navigationHistory[index - 2].page->Name();
                        const NavigationRoute* const incomingRoute = this->navigationHistory[index - 1].incomingRoute;
                        backwardOfRouteId = incomingRoute == nullptr ? "" : incomingRoute->id;
                        break;
                    }
                }
            } else {
                target = navigationRoute.target;
            }
            if (!target.empty()) {
                std::string previewDefault = "null";
                if (navigationRoute.dataContract != nullptr) {
                    previewDefault = navigationRoute.dataContract->preview_CreatePreviewDefaultFn()->Serialize();
                }
                result.push_back({
                    navigationRoute.id,
                    navigationRoute.source,
                    target,
                    backwardOfRouteId,
                    navigationRoute.title,
                    navigationRoute.isDefault,
                    navigationRoute.targetKind,
                    navigationRoute.dataContract == nullptr ? "" : navigationRoute.dataContract->typeId,
                    std::move(previewDefault),
                });
            }
        }
        return result;
    }

    std::string_view PageManager::preview_PageTitle(std::string_view pageName) const {
        const interface::IPage* const page = this->pages.Find(pageName);
        return page == nullptr ? pageName : page->preview_GraphTitle();
    }

    bool PageManager::preview_ApplyScenario(std::string_view pageName, std::string_view json, std::string& error) {
        interface::IPage* const page = this->pages.Find(pageName);
        if (page == nullptr) {
            error = "Unknown MobileClock page";
            return false;
        }
        return page->preview_ApplyScenario(json, error);
    }

    bool PageManager::preview_ReloadMarkup(std::string_view pageName, std::string_view markup,
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
                auto runtimeBindingContext = page->preview_RuntimeContext();
                // Смещение прокрутки восстанавливается после замены шаблона: новый
                // ScrollViewer не наследует состояние прежнего экземпляра.
                const auto previewRuntimePanState = this->inputDispatcher.preview_CaptureRuntimePan();
                runtimeBindingContext.prepareTree = [](xaml::Element& root) {
                    xaml::AnimationRegistry animationRegistry;
                    mobileclock::presentation::core::RegisterAnimations(animationRegistry);
                    animationRegistry.ValidateTree(root);
                    xaml::AnimationController validation;
                    validation.Attach(root, animationRegistry);
                };
                runtimeBindingContext.beforeCommit = [this]() { this->inputDispatcher.Cancel(); };
                if (!control->ReplaceTemplate(ast, runtimeBindingContext, diagnostics)) {
                    return false;
                }
                this->inputDispatcher.preview_RestoreRuntimePan(page->Root(), previewRuntimePanState);
                xaml::layout(page->Root(), this->availableSize);
                return true;
            }
            auto runtimeBindingContext = page->preview_RuntimeContext();
            // Prepare строит и проверяет отдельное дерево. Если парсинг, биндинг или
            // валидация завершаются ошибкой, текущая страница остаётся без изменений.
            auto result = xaml::runtime::RuntimeReloadTransaction{}.Prepare(
                markup, sourcePath, runtimeBindingContext, page->Root(), this->availableSize);
            xaml::AnimationRegistry animationRegistry;
            mobileclock::presentation::core::RegisterAnimations(animationRegistry);
            result.root->SetAnimationParametersProvider([this]() {
                return xaml::AnimationParameters::Create(mobileclock::presentation::core::PageTransitionData{
                    this->outgoingPage == nullptr ? "" : std::string(this->outgoingPage->Name()),
                    this->currentPage == nullptr ? "" : std::string(this->currentPage->Name()),
                    this->navigationDirection});
            });
            this->animationController.Attach(*result.root, animationRegistry);
            animationRegistry.ValidateTree(*result.root);
            if (runtimeBindingContext.prepareTree) {
                runtimeBindingContext.prepareTree(*result.root);
            }
            xaml::layout(*result.root, this->availableSize);
            // Отменяем активный жест до передачи владения новым деревом, поскольку
            // InputDispatcher мог хранить указатель на элемент старого дерева.
            const auto previewRuntimePanState = this->inputDispatcher.preview_CaptureRuntimePan();
            this->inputDispatcher.Cancel();
            // Это единственная точка, где полная перезагрузка страницы становится видимой.
            page->preview_ReplaceRuntimeTree(std::move(result));
            this->inputDispatcher.preview_RestoreRuntimePan(page->Root(), previewRuntimePanState);
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
        this->inputDispatcher.PointerDown(this->currentPage->Root(), x, y, &this->animationController);
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
            this->animationController);
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
        this->animationController.Update();
        this->inputDispatcher.Update(this->currentPage->Root(), this->animationController);
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
        typename TData,
        NavigationTrigger TTrigger,
        mobileclock::presentation::core::NavigationDirection TDirection
    >
    PageManager::NavigationRoute PageManager::MakeRoute(std::string_view id, std::string_view title, bool isDefault) {
        const NavigationDataContract* dataContract = nullptr;
        if constexpr (!std::is_same_v<TData, NoNavigationData>) {
            dataContract = &TData::Contract();
        }
        return {
            id,
            TSource::PageName,
            TTrigger,
            TTarget::PageName,
            NavigationTargetKind::page,
            dataContract,
            TDirection,
            title,
            isDefault,
        };
    }

    template <
        typename TSource,
        typename TData,
        NavigationTrigger TTrigger,
        mobileclock::presentation::core::NavigationDirection TDirection
    >
    PageManager::NavigationRoute PageManager::MakeBackRoute(std::string_view id, std::string_view title, bool isDefault) {
        const NavigationDataContract* dataContract = nullptr;
        if constexpr (!std::is_same_v<TData, NoNavigationData>) {
            dataContract = &TData::Contract();
        }
        return {
            id,
            TSource::PageName,
            TTrigger,
            {},
            NavigationTargetKind::previousPage,
            dataContract,
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
            MakeRoute<
                ui::page::MainPageViewModel,
                ui::page::AddAlarmPageViewModel,
                NoNavigationData,
                NavigationTrigger::createAlarm,
                mobileclock::presentation::core::NavigationDirection::forward
            >(
                "main-create-alarm",
                "Новый будильник"
            ),
            MakeRoute<
                ui::page::MainPageViewModel,
                ui::page::AddAlarmPageViewModel,
                AlarmEditNavigationState,
                NavigationTrigger::editAlarm,
                mobileclock::presentation::core::NavigationDirection::forward
            >(
                "main-edit-alarm",
                "Изменить будильник",
                false
            ),
            MakeRoute<
                ui::page::MainPageViewModel,
                ui::page::SettingsPageViewModel,
                NoNavigationData,
                NavigationTrigger::navigateToSettings,
                mobileclock::presentation::core::NavigationDirection::forward
            >(
                "main-open-settings",
                "Открыть настройки"
            ),
            MakeBackRoute<
                ui::page::AddAlarmPageViewModel,
                NoNavigationData,
                NavigationTrigger::navigateBack,
                mobileclock::presentation::core::NavigationDirection::backward
            >(
                "add-alarm-back",
                "Вернуться назад"
            ),
#if defined(ANDROID_APP_PREVIEWER)
            MakeRoute<
                ui::page::AddAlarmPageViewModel,
                ui::page::preview_XiaomiThemesPageViewModel,
                NoNavigationData,
                NavigationTrigger::preview_ChooseAlarmMelody,
                mobileclock::presentation::core::NavigationDirection::forward
            >(
                "add-alarm-choose-melody",
                "Выбрать мелодию"
            ),
            MakeBackRoute<
                ui::page::preview_XiaomiThemesPageViewModel,
                preview_AlarmMelodyNavigationState,
                NavigationTrigger::navigateBack,
                mobileclock::presentation::core::NavigationDirection::backward
            >(
                "xiaomi-themes-back",
                "Вернуться назад"
            ),
#endif
            MakeBackRoute<
                ui::page::SettingsPageViewModel,
                NoNavigationData,
                NavigationTrigger::navigateBack,
                mobileclock::presentation::core::NavigationDirection::backward
            >(
                "settings-back",
                "Вернуться назад"
            ),
        };
        return routes;
    }

#if defined(ANDROID_APP_PREVIEWER)
    bool PageManager::preview_ExecuteRoute(std::span<const NavigationRoute*> navigationRoutes, std::string& error) {
        for (const NavigationRoute* edge : navigationRoutes) {
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
            // Тип маршрута владеет preview-default, поэтому previewer не знает, как
            // конструировать бизнес-данные и не создаёт специальных dummy-объектов.
            std::unique_ptr<base::NavigationStateBase> state = edge->dataContract == nullptr || !edge->dataContract->isRequired
                ? nullptr
                : edge->dataContract->preview_CreatePreviewDefaultFn();
            const bool navigated = this->Trigger(edge->trigger, std::move(state));
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