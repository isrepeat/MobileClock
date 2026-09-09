#include "UI/PageManager.h"

#include <XamlRuntime/Input.h>
#include <XamlRuntime/RenderEngine.h>

#include "MobileClock.Presentation/AnimationRenderers.h"
#include "MobileClock.Presentation/PageTransition.h"
#include "MobileClock.Presentation/Registrations.h"
#include "MobileClock.UI/Controls/AlarmList.h"

namespace mobileclock::ui {
    PageManager::PageManager(IApplicationActions& actions)
        : mainPageViewModel(*this, actions, [this]() {
            this->RefreshMainPage();
        })
        , settingsPageViewModel(*this, actions) {
    }

    //
    // IPageNavigator
    //
    void PageManager::Navigate(Page page) {
        if (this->currentPage == page || this->isTransitioning) {
            return;
        }
        this->outgoingPage = this->currentPage;
        this->currentPage = page;
        this->mainPageViewModel.Root().SetVisibility(page == Page::main
            ? xaml::attr::Visibility::visible : xaml::attr::Visibility::collapsed);
        this->settingsPageViewModel.Root().SetVisibility(page == Page::settings
            ? xaml::attr::Visibility::visible : xaml::attr::Visibility::collapsed);
        this->isTransitioning = xaml::AnimationController::IsAnimating(this->mainPageViewModel.Root())
            || xaml::AnimationController::IsAnimating(this->settingsPageViewModel.Root());
    }

    //
    // API
    //
    void PageManager::Initialize(xaml::Size availableSize) {
        this->touchHandler.CancelTouch();
        this->animations = xaml::AnimationController{};
        this->currentPage = Page::main;
        this->isTransitioning = false;
        this->availableSize = availableSize;
        this->mainPageViewModel.Initialize(this->availableSize);
        this->settingsPageViewModel.Initialize(availableSize);
        xaml::AnimationRegistry registry;
        mobileclock::presentation::RegisterAnimations(registry);
        const auto parameters = [this]() {
            return xaml::AnimationParameters::Create(presentation::PageTransitionData{
                this->outgoingPage == Page::main ? "main" : "settings",
                this->currentPage == Page::main ? "main" : "settings",
                this->currentPage == Page::settings ? presentation::NavigationDirection::forward : presentation::NavigationDirection::backward,
            });
        };
        auto& main = this->mainPageViewModel.Root();
        auto& settings = this->settingsPageViewModel.Root();
        main.SetAnimationParametersProvider(parameters);
        settings.SetAnimationParametersProvider(parameters);
        settings.SetVisibility(xaml::attr::Visibility::collapsed);
        this->animations.Attach(main, registry);
        this->animations.Attach(settings, registry);
    }

    void PageManager::SetStatus(std::string value) {
        this->mainPageViewModel.SetStatus(std::move(value));
    }

    void PageManager::HandleTouchDown(float x, float y) {
        if (this->isTransitioning || this->pendingAlarmDeletion != nullptr) {
            return;
        }
        if (this->currentPage == Page::main) {
            this->touchHandler.HandleTouchDown(this->mainPageViewModel.Root(), x, y, &this->animations);
            return;
        }
        this->touchHandler.HandleTouchDown(this->settingsPageViewModel.Root(), x, y);
    }

    bool PageManager::HandleTouchMove(float x, float y) {
        if (this->isTransitioning
            || this->pendingAlarmDeletion != nullptr
            || this->currentPage != Page::main) {
            return false;
        }
        return this->touchHandler.HandleTouchMove(x, y);
    }

    bool PageManager::HandleTouchUp(float x, float y) {
        if (this->isTransitioning || this->pendingAlarmDeletion != nullptr) {
            return false;
        }
        xaml::Element& root = this->currentPage == Page::main
            ? this->mainPageViewModel.Root()
            : this->settingsPageViewModel.Root();
        const void* swipedDataContext = nullptr;
        xaml::Element* const element = this->touchHandler.HandleTouchUp(
            root,
            x,
            y,
            this->animations,
            swipedDataContext);
        if (this->currentPage == Page::main && swipedDataContext != nullptr) {
            this->pendingAlarmDeletion = swipedDataContext;
            this->pendingAlarmDeletionAt = std::chrono::steady_clock::now()
                + std::chrono::milliseconds(220);
            return true;
        }
        if (element == nullptr) {
            return false;
        }
        if (this->currentPage == Page::main) {
            this->mainPageViewModel.HandleTap(*element);
            return true;
        }

        this->settingsPageViewModel.HandleTap(*element);
        return true;
    }

    void PageManager::CancelTouch() {
        this->touchHandler.CancelTouch();
    }

    int PageManager::CursorKind(float x, float y) {
        xaml::Element& root = this->currentPage == Page::main
            ? this->mainPageViewModel.Root()
            : this->settingsPageViewModel.Root();
        xaml::Element* const interactive = xaml::HitTest(root, x, y);
        if (interactive != nullptr && interactive->Type() != xaml::ElementType::scrollViewer) {
            return 1;
        }
        xaml::Element* visual = xaml::HitTestVisual(root, x, y);
        for (; visual != nullptr; visual = visual->Parent()) {
            if (visual->Type() == xaml::ElementType::scrollViewer) {
                return 2;
            }
        }
        return 0;
    }

    void PageManager::RefreshMainPage() {
        this->touchHandler.CancelTouch();
        this->mainPageViewModel.Initialize(this->availableSize);
        xaml::AnimationRegistry registry;
        mobileclock::presentation::RegisterAnimations(registry);
        this->animations.Attach(this->mainPageViewModel.Root(), registry);
        this->mainPageViewModel.Root().SetAnimationParametersProvider([this]() {
            return xaml::AnimationParameters::Create(presentation::PageTransitionData{
                this->outgoingPage == Page::main ? "main" : "settings",
                this->currentPage == Page::main ? "main" : "settings",
                this->currentPage == Page::settings ? presentation::NavigationDirection::forward : presentation::NavigationDirection::backward,
            });
        });
    }

    void PageManager::UpdateClock() {
        this->animations.Update();
        this->touchHandler.Update();
        if (this->pendingAlarmDeletion != nullptr
            && std::chrono::steady_clock::now() >= this->pendingAlarmDeletionAt) {
            const void* const alarm = this->pendingAlarmDeletion;
            this->pendingAlarmDeletion = nullptr;
            const controls::AlarmList::RemovalState state = this->mainPageViewModel.AlarmList()
                .CaptureRemovalState(alarm);
            if (state.isPresent && this->mainPageViewModel.HandleSwipe(alarm)) {
                this->mainPageViewModel.AlarmList().RestoreViewportAndAnimate(
                    state,
                    this->mainPageViewModel.Root(),
                    this->animations,
                    std::chrono::milliseconds(840));
            }
        }
        if (this->isTransitioning
            && !xaml::AnimationController::IsAnimating(this->mainPageViewModel.Root())
            && !xaml::AnimationController::IsAnimating(this->settingsPageViewModel.Root())) {
            this->isTransitioning = false;
        }
        this->mainPageViewModel.UpdateClock();
    }

    void PageManager::Render(
        xaml::IRenderBackend& renderer,
        const xaml::RendererRegistry& renderers) const {
        if (this->isTransitioning) {
            if (this->outgoingPage == Page::main) {
                this->mainPageViewModel.Render(renderer, renderers);
            } else {
                this->settingsPageViewModel.Render(renderer, renderers);
            }
        }
        if (this->currentPage == Page::main) {
            this->mainPageViewModel.Render(renderer, renderers);
            return;
        }
        this->settingsPageViewModel.Render(renderer, renderers);
    }
}