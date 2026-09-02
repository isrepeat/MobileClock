#include "UI/PageManager.h"

#include <XamlRuntime/RenderEngine.h>

namespace mobileclock::ui {
    void PageManager::Initialize(xaml::Size availableSize) {
        this->currentPage = Page::main;
        this->mainPageViewModel.Initialize(availableSize);
        this->settingsPageViewModel.Initialize(availableSize);
    }

    void PageManager::HandleTouchDown(float x, float y) {
        if (this->currentPage == Page::main) {
            this->mainPageViewModel.HandleTouchDown(x, y);
            return;
        }
        this->settingsPageViewModel.HandleTouchDown(x, y);
    }

    bool PageManager::HandleTouchUp(float x, float y) {
        if (this->currentPage == Page::main) {
            const MainPageViewModel::TouchAction action = this->mainPageViewModel.HandleTouchUp(x, y);
            if (action == MainPageViewModel::TouchAction::navigateToSettings) {
                this->currentPage = Page::settings;
                return true;
            }
            return action == MainPageViewModel::TouchAction::contentChanged;
        }

        if (this->settingsPageViewModel.HandleTouchUp(x, y)
            == SettingsPageViewModel::TouchAction::navigateToMain) {
            this->currentPage = Page::main;
            return true;
        }
        return false;
    }

    void PageManager::CancelTouch() {
        if (this->currentPage == Page::main) {
            this->mainPageViewModel.CancelTouch();
            return;
        }
        this->settingsPageViewModel.CancelTouch();
    }

    void PageManager::UpdateClock() {
        this->mainPageViewModel.UpdateClock();
    }

    void PageManager::Render(xaml::IRenderBackend& renderer) const {
        if (this->currentPage == Page::main) {
            this->mainPageViewModel.Render(renderer);
            return;
        }
        this->settingsPageViewModel.Render(renderer);
    }
}