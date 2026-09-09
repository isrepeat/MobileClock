#pragma once
#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Animation.h>

#include "UI/Pages/SettingsPageViewModel.h"
#include "UI/ApplicationActions.h"
#include "UI/Pages/MainPageViewModel.h"
#include "UI/TouchHandler.h"
#include "UI/Navigation.h"

#include <chrono>
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
        void Navigate(Page page) override;

        void Initialize(xaml::Size availableSize);
        void SetStatus(std::string value);
        void HandleTouchDown(float x, float y);
        bool HandleTouchMove(float x, float y);
        bool HandleTouchUp(float x, float y);
        void CancelTouch();
        void UpdateClock();
        void Render(xaml::IRenderBackend& renderer, const xaml::RendererRegistry& renderers) const;

    private:
        void RefreshMainPage();

    private:
        xaml::Size availableSize;
        Page currentPage = Page::main;
        Page outgoingPage = Page::main;
        bool isTransitioning = false;
        const void* pendingAlarmDeletion = nullptr;
        std::chrono::steady_clock::time_point pendingAlarmDeletionAt;
        xaml::AnimationController animations;
        TouchHandler touchHandler;
        MainPageViewModel mainPageViewModel;
        SettingsPageViewModel settingsPageViewModel;
    };
}