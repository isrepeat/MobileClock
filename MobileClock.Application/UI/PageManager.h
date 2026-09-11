#pragma once
#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Animation.h>

#include "UI/Pages/StatisticsPageViewModel.h"
#include "UI/Pages/SettingsPageViewModel.h"
#include "UI/Pages/MainPageViewModel.h"
#include "UI/InputDispatcher.h"
#include "UI/PageRegistry.h"

#include <string_view>
#include <string>

namespace xaml {
    class IRenderBackend;
    class RendererRegistry;
}

namespace mobileclock::ui {
    class PageManager final : public IPageNavigator {
    public:
        explicit PageManager(IApplicationActions& actions);
        ~PageManager() = default;

        PageManager(const PageManager&) = delete;
        PageManager& operator=(const PageManager&) = delete;

        //
        // IPageNavigator
        //
        bool Navigate(std::string_view pageName) override;

        void Initialize(xaml::Size availableSize);
        void SetAnimationPlaybackRate(float value);
        void SetStatus(std::string value);
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        bool ApplyPreviewScenario(std::string_view page, std::string_view json, std::string& error);
        bool ReloadMarkup(std::string_view page, std::string_view markup, std::string_view sourcePath, std::string& diagnostics);
#endif
        void HandleTouchDown(float x, float y);
        bool HandleTouchMove(float x, float y);
        bool HandleTouchUp(float x, float y);
        void CancelTouch();
        xaml::Element& Root();
        void UpdateClock();
        void Render(xaml::IRenderBackend& renderer, const xaml::RendererRegistry& renderers) const;

    private:
        using ApplicationPages = PageRegistry<
            MainPageViewModel,
            SettingsPageViewModel,
            StatisticsPageViewModel>;

    private:
        xaml::Size availableSize;
        PageContext pageContext;
        ApplicationPages pages;
        IPage* currentPage = nullptr;
        IPage* outgoingPage = nullptr;
        bool isTransitioning = false;
        xaml::AnimationController animations;
        InputDispatcher inputDispatcher;
    };
}