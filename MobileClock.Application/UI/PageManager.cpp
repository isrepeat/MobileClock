#include "UI/PageManager.h"

#include <Helpers.Logging/Logging.h>
#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/IRuntimeReloadableControl.h>
#include <XamlRuntime/RuntimeMarkup/RuntimeReloadTransaction.h>
#include <XamlRuntime/RuntimeMarkup/XamlParser.h>
#endif
#include <XamlRuntime/RenderEngine.h>

#include "MobileClock.Presentation/AnimationRenderers.h"
#include "MobileClock.Presentation/PageTransition.h"
#include "MobileClock.Presentation/Registrations.h"

#include <algorithm>
#include <format>
#include <limits>
#include <vector>
#include <array>

namespace mobileclock::ui {
    PageManager::PageManager(IApplicationActions& actions)
        : pageContext{static_cast<IPageNavigator&>(*this), actions}
        , pages(this->pageContext) {
        this->pageContext.saveAlarm = [this](const AlarmSettings& settings) {
            this->pages.Get<MainPageViewModel>().AddAlarm(settings);
        };
    }

    //
    // IPageNavigator
    //
    bool PageManager::Navigate(std::string_view pageName) {
        IPage* const page = this->pages.Find(pageName);
        if (page == nullptr || this->currentPage == page || this->isTransitioning) {
            return page != nullptr;
        }
        this->outgoingPage = this->currentPage;
        this->currentPage = page;
        this->pages.ForEach([page](IPage& candidate) {
            candidate.Root().SetVisibility(&candidate == page
                ? xaml::attr::Visibility::visible
                : xaml::attr::Visibility::collapsed);
        });
        this->isTransitioning = this->pages.IsAnyAnimating();
        return true;
    }

    bool PageManager::Trigger(NavigationTrigger trigger) {
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
        IPage* const target = this->pages.Find(route->target);
        if (target == nullptr) {
            LOG_WARNING(
                "MobileClock.Navigation",
                "Route target '{}' is not registered",
                route->target);
            return false;
        }
        const NavigationRequest request{route->source, route->target, route->trigger};
        std::unique_ptr<NavigationState> state = this->currentPage->OnNavigatingFrom(request);
        if (!target->OnNavigatingTo(request, std::move(state))) {
            LOG_WARNING(
                "MobileClock.Navigation",
                "Route preparation failed for {} -> {}",
                route->source,
                route->target);
            return false;
        }
        return this->Navigate(route->target);
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
        mobileclock::presentation::RegisterAnimations(registry);
        const auto parameters = [this]() {
            return xaml::AnimationParameters::Create(presentation::PageTransitionData{
                this->outgoingPage == nullptr ? "" : std::string(this->outgoingPage->Name()),
                this->currentPage == nullptr ? "" : std::string(this->currentPage->Name()),
                this->currentPage == &this->pages.GetPage<MainPageViewModel>()
                    ? presentation::NavigationDirection::backward
                    : presentation::NavigationDirection::forward,
            });
        };
        this->pages.ForEach([&](IPage& page) {
            page.Initialize(this->availableSize);
            page.Root().SetAnimationParametersProvider(parameters);
            page.Root().SetVisibility(&page == &this->pages.GetPage<MainPageViewModel>()
                ? xaml::attr::Visibility::visible
                : xaml::attr::Visibility::collapsed);
            this->animations.Attach(page.Root(), registry);
        });
        this->currentPage = &this->pages.GetPage<MainPageViewModel>();
        this->outgoingPage = this->currentPage;
        this->currentPage->Root().SetVisibility(xaml::attr::Visibility::visible);
        this->isTransitioning = false;
    }

    void PageManager::Resize(xaml::Size availableSize) {
        this->availableSize = availableSize;
        this->pages.ForEach([availableSize](IPage& page) {
            xaml::layout(page.Root(), availableSize);
        });
    }

    void PageManager::SetAnimationPlaybackRate(float value) {
        this->animations.SetPlaybackRate(value);
    }

    void PageManager::SetStatus(std::string value) {
        this->pages.Get<MainPageViewModel>().SetStatus(std::move(value));
    }

    void PageManager::AddAlarmMelody(std::string name, std::string uri) {
        this->pages.Get<AddAlarmPageViewModel>().AddMelody(std::move(name), std::move(uri));
    }

    void PageManager::SetAlarmMelody(std::string name, std::string uri) {
        AddAlarmPageViewModel& page = this->pages.Get<AddAlarmPageViewModel>();
        page.SetMelody(std::move(name), std::move(uri));
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

    bool PageManager::NavigatePreviewRoute(std::span<const std::string_view> path, std::string& error) {
        error.clear();
        if (this->currentPage == nullptr || path.empty()) {
            error = "Preview route requires an initialized application session and a path";
            LOG_WARNING("MobileClock.PreviewRoute", "Explicit route rejected: {}", error);
            return false;
        }
        std::string serializedPath;
        for (const std::string_view page : path) {
            if (!serializedPath.empty()) {
                serializedPath += '>';
            }
            serializedPath += page;
        }
        LOG_INFO(
            "MobileClock.PreviewRoute",
            "Explicit route request: current='{}', path='{}'",
            this->currentPage->Name(),
            serializedPath);
        if (path.front() != this->currentPage->Name()) {
            error = std::format("Preview route starts at {}, but the active page is {}", path.front(), this->currentPage->Name());
            LOG_WARNING("MobileClock.PreviewRoute", "Explicit route rejected: {}", error);
            return false;
        }
        std::vector<const NavigationRoute*> route;
        for (size_t index = 1; index < path.size(); ++index) {
            const auto edge = std::find_if(Routes().begin(), Routes().end(), [&](const NavigationRoute& candidate) {
                return candidate.source == path[index - 1] && candidate.target == path[index];
            });
            if (edge == Routes().end()) {
                error = std::format("No preview transition from {} to {}", path[index - 1], path[index]);
                LOG_WARNING("MobileClock.PreviewRoute", "Explicit route rejected: {}", error);
                return false;
            }
            route.push_back(&*edge);
        }
        return this->ExecutePreviewRoute(route, error);
    }

    std::string PageManager::PreviewRouteGraph() const {
        std::string result;
        for (const NavigationRoute& route : Routes()) {
            if (!result.empty()) {
                result += ';';
            }
            result += route.source;
            result += '>';
            result += route.target;
        }
        return result;
    }

    std::string_view PageManager::PreviewPageTitle(std::string_view pageName) const {
        const IPage* const page = this->pages.Find(pageName);
        return page == nullptr ? pageName : page->PreviewGraphTitle();
    }

    bool PageManager::ApplyPreviewScenario(std::string_view pageName, std::string_view json, std::string& error) {
        IPage* const page = this->pages.Find(pageName);
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
            IPage* const page = this->pages.Find(pageName);
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
                    mobileclock::presentation::RegisterAnimations(registry);
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
            mobileclock::presentation::RegisterAnimations(registry);
            result.root->SetAnimationParametersProvider([this]() {
                return xaml::AnimationParameters::Create(presentation::PageTransitionData{
                    this->outgoingPage == nullptr ? "" : std::string(this->outgoingPage->Name()),
                    this->currentPage == nullptr ? "" : std::string(this->currentPage->Name()),
                    this->currentPage == &this->pages.GetPage<MainPageViewModel>()
                        ? presentation::NavigationDirection::backward : presentation::NavigationDirection::forward});
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
        this->pages.ForEach([](IPage& page) {
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
    template <typename TSource, typename TTarget, NavigationTrigger TTrigger>
    PageManager::NavigationRoute PageManager::MakeRoute() {
        return {
            TSource::PageName,
            TTrigger,
            TTarget::PageName,
        };
    }

    std::span<const PageManager::NavigationRoute> PageManager::Routes() {
        static const std::array routes{
            MakeRoute<MainPageViewModel, AddAlarmPageViewModel, NavigationTrigger::createAlarm>(),
            MakeRoute<MainPageViewModel, SettingsPageViewModel, NavigationTrigger::navigateToSettings>(),
            MakeRoute<AddAlarmPageViewModel, MainPageViewModel, NavigationTrigger::navigateToMain>(),
#if defined(MOBILECLOCK_XAML_PREVIEWER)
            MakeRoute<AddAlarmPageViewModel, XiaomiThemesPageViewModel, NavigationTrigger::chooseAlarmMelody>(),
            MakeRoute<XiaomiThemesPageViewModel, AddAlarmPageViewModel, NavigationTrigger::applySelectedMelody>(),
#endif
            MakeRoute<SettingsPageViewModel, MainPageViewModel, NavigationTrigger::navigateToMain>(),
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
            LOG_INFO(
                "MobileClock.PreviewRoute",
                "Executing transition: {} -> {}",
                edge->source,
                edge->target);
            this->isTransitioning = false;
            if (!this->Trigger(edge->trigger)) {
                error = std::format("Preview transition {} -> {} was rejected", edge->source, edge->target);
                LOG_ERROR("MobileClock.PreviewRoute", "Route execution failed: {}", error);
                return false;
            }
            const std::string_view currentPage = this->CurrentPageName();
            if (currentPage != edge->target) {
                error = std::format(
                    "Preview transition {} -> {} ended on {}",
                    edge->source,
                    edge->target,
                    currentPage);
                LOG_ERROR(
                    "MobileClock.PreviewRoute",
                    "Route execution failed after '{}': expected='{}', actual='{}', previous='{}'",
                    error,
                    edge->target,
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