#pragma once

#include "UI/MainPageViewModel.h"
#include "UI/SettingsPageViewModel.h"

#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Animation.h>

#include <chrono>

namespace xaml {
    class IRenderBackend;
}

namespace mobileclock::ui {
    class PageManager final {
    public:
        PageManager() = default;
        ~PageManager() = default;

        PageManager(const PageManager&) = delete;
        PageManager& operator=(const PageManager&) = delete;

        void Initialize(xaml::Size availableSize);
        void HandleTouchDown(float x, float y);
        bool HandleTouchUp(float x, float y);
        void CancelTouch();
        void UpdateClock();
        void Render(xaml::IRenderBackend& renderer) const;

    private:
        enum class Page {
            main,
            settings,
        };

    private:
        Page currentPage = Page::main;
        Page outgoingPage = Page::main;
        bool isTransitioning = false;
        std::chrono::steady_clock::time_point transitionEndsAt{};
        xaml::AnimationController animations;
        MainPageViewModel mainPageViewModel;
        SettingsPageViewModel settingsPageViewModel;
    };
}