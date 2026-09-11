#include "UI/PageManager.h"

#include <XamlRuntime/RenderEngine.h>
#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/IRuntimeReloadableControl.h>
#include <XamlRuntime/RuntimeMarkup/RuntimeReloadTransaction.h>
#include <XamlRuntime/RuntimeMarkup/XamlParser.h>
#endif

#include "MobileClock.Presentation/AnimationRenderers.h"
#include "MobileClock.Presentation/PageTransition.h"
#include "MobileClock.Presentation/Registrations.h"

namespace mobileclock::ui {
    PageManager::PageManager(IApplicationActions& actions)
        : pageContext{static_cast<IPageNavigator&>(*this), actions}
        , pages(this->pageContext) {
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
            page.Root().SetVisibility(xaml::attr::Visibility::collapsed);
            this->animations.Attach(page.Root(), registry);
        });
        this->currentPage = &this->pages.GetPage<MainPageViewModel>();
        this->outgoingPage = this->currentPage;
        this->currentPage->Root().SetVisibility(xaml::attr::Visibility::visible);
        this->isTransitioning = false;
    }

    void PageManager::SetAnimationPlaybackRate(float value) {
        this->animations.SetPlaybackRate(value);
    }

    void PageManager::SetStatus(std::string value) {
        this->pages.Get<MainPageViewModel>().SetStatus(std::move(value));
    }

#if defined(MOBILECLOCK_XAML_PREVIEWER)
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
}