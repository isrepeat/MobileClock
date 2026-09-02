#pragma once

#include "UI/MainPageViewModel.h"
#include "UI/SettingsPageViewModel.h"

#include <XamlRuntime/XamlLayout.h>

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
        MainPageViewModel mainPageViewModel;
        SettingsPageViewModel settingsPageViewModel;
    };
}