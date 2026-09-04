#include "UI/PageManager.h"

#include <XamlRuntime/RenderEngine.h>

#include <chrono>

namespace mobileclock::ui {
    void PageManager::Initialize(xaml::Size availableSize) {
        this->currentPage = Page::main;
        this->isTransitioning = false;
        this->mainPageViewModel.Initialize(availableSize);
        this->settingsPageViewModel.Initialize(availableSize);
    }

    void PageManager::HandleTouchDown(float x, float y) {
        if (this->currentPage == Page::main) {
            this->mainPageViewModel.HandleTouchDown(x, y, this->animations);
            return;
        }
        this->settingsPageViewModel.HandleTouchDown(x, y);
    }

    bool PageManager::HandleTouchUp(float x, float y) {
        if (this->isTransitioning) {
            return false;
        }
        if (this->currentPage == Page::main) {
            const MainPageViewModel::TouchAction action = this->mainPageViewModel.HandleTouchUp(
                x,
                y,
                this->animations);
            if (action == MainPageViewModel::TouchAction::navigateToSettings) {
                const float width = this->mainPageViewModel.Root().Bounds().width;
                this->outgoingPage = Page::main;
                this->currentPage = Page::settings;
                this->animations.Animate(
                    this->mainPageViewModel.Root(),
                    xaml::AnimatedProperty::renderOffsetX,
                    0.0f,
                    -width,
                    std::chrono::milliseconds(240));
                this->animations.Animate(
                    this->mainPageViewModel.Root(),
                    xaml::AnimatedProperty::opacity,
                    1.0f,
                    0.0f,
                    std::chrono::milliseconds(180));
                this->animations.Animate(
                    this->settingsPageViewModel.Root(),
                    xaml::AnimatedProperty::renderOffsetX,
                    width,
                    0.0f,
                    std::chrono::milliseconds(240));
                this->animations.Animate(
                    this->settingsPageViewModel.Root(),
                    xaml::AnimatedProperty::opacity,
                    0.0f,
                    1.0f,
                    std::chrono::milliseconds(180));
                this->isTransitioning = true;
                this->transitionEndsAt = std::chrono::steady_clock::now()
                    + std::chrono::milliseconds(240);
                return true;
            }
            return action == MainPageViewModel::TouchAction::contentChanged;
        }

        if (this->settingsPageViewModel.HandleTouchUp(x, y, this->animations)
            == SettingsPageViewModel::TouchAction::navigateToMain) {
            const float width = this->settingsPageViewModel.Root().Bounds().width;
            this->outgoingPage = Page::settings;
            this->currentPage = Page::main;
            this->animations.Animate(
                this->settingsPageViewModel.Root(),
                xaml::AnimatedProperty::renderOffsetX,
                0.0f,
                width,
                std::chrono::milliseconds(240));
            this->animations.Animate(
                this->settingsPageViewModel.Root(),
                xaml::AnimatedProperty::opacity,
                1.0f,
                0.0f,
                std::chrono::milliseconds(180));
            this->animations.Animate(
                this->mainPageViewModel.Root(),
                xaml::AnimatedProperty::renderOffsetX,
                -width,
                0.0f,
                std::chrono::milliseconds(240));
            this->animations.Animate(
                this->mainPageViewModel.Root(),
                xaml::AnimatedProperty::opacity,
                0.0f,
                1.0f,
                std::chrono::milliseconds(180));
            this->isTransitioning = true;
            this->transitionEndsAt = std::chrono::steady_clock::now()
                + std::chrono::milliseconds(240);
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
        this->animations.Update();
        if (this->isTransitioning && std::chrono::steady_clock::now() >= this->transitionEndsAt) {
            this->isTransitioning = false;
        }
        this->mainPageViewModel.UpdateClock();
    }

    void PageManager::Render(xaml::IRenderBackend& renderer) const {
        if (this->isTransitioning) {
            if (this->outgoingPage == Page::main) {
                this->mainPageViewModel.Render(renderer);
            } else {
                this->settingsPageViewModel.Render(renderer);
            }
        }
        if (this->currentPage == Page::main) {
            this->mainPageViewModel.Render(renderer);
            return;
        }
        this->settingsPageViewModel.Render(renderer);
    }
}