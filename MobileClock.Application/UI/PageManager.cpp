#include "UI/PageManager.h"

#include <XamlRuntime/RenderEngine.h>

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