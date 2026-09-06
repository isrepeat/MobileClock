#include <XamlRuntime/RenderEngine.h>

#include "Renderer/AnimationRenderers.h"
#include "UI/PageTransition.h"
#include "UI/PageManager.h"

namespace mobileclock::ui {
    //
    // API
    //
    void PageManager::Initialize(xaml::Size availableSize) {
        this->touchHandler.CancelTouch();
        this->animations = xaml::AnimationController{};
        this->currentPage = Page::main;
        this->isTransitioning = false;
        this->mainPageViewModel.Initialize(availableSize);
        this->settingsPageViewModel.Initialize(availableSize);
        xaml::AnimationRegistry registry = mobileclock::resources::effects::CreateAnimations();
        renderer::RegisterAnimations(registry);
        const auto parameters = [this]() {
            return xaml::AnimationParameters::Create(PageTransitionData{
                this->outgoingPage == Page::main ? "main" : "settings",
                this->currentPage == Page::main ? "main" : "settings",
                this->currentPage == Page::settings ? NavigationDirection::forward : NavigationDirection::backward,
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

    void PageManager::SetCommandHandler(std::function<void(const std::string&)> handler) {
        this->commandHandler = std::move(handler);
        const auto dispatch = [this](const std::string& command) {
            if (this->commandHandler) {
                this->commandHandler(command);
            }
        };
        this->mainPageViewModel.BindCommand("createAlarm", [dispatch]() { dispatch("createAlarm"); });
        this->mainPageViewModel.BindCommand("toggleAlarm", [dispatch]() { dispatch("toggleAlarm"); });
        this->mainPageViewModel.BindCommand("updateApplication", [dispatch]() { dispatch("updateApplication"); });
        this->mainPageViewModel.BindCommand("uploadScreenshot", [dispatch]() { dispatch("uploadScreenshot"); });
        this->settingsPageViewModel.BindCommand("shareLogs", [dispatch]() { dispatch("shareLogs"); });
        this->settingsPageViewModel.BindCommand("exportLogs", [dispatch]() { dispatch("exportLogs"); });
    }

    void PageManager::SetStatus(std::string value) {
        this->mainPageViewModel.SetStatus(std::move(value));
    }

    void PageManager::HandleTouchDown(float x, float y) {
        if (this->isTransitioning) {
            return;
        }
        if (this->currentPage == Page::main) {
            this->touchHandler.HandleTouchDown(this->mainPageViewModel.Root(), x, y, &this->animations);
            return;
        }
        this->touchHandler.HandleTouchDown(this->settingsPageViewModel.Root(), x, y);
    }

    bool PageManager::HandleTouchUp(float x, float y) {
        if (this->isTransitioning) {
            return false;
        }
        xaml::Element& root = this->currentPage == Page::main
            ? this->mainPageViewModel.Root()
            : this->settingsPageViewModel.Root();
        xaml::Element* const element = this->touchHandler.HandleTouchUp(root, x, y, this->animations);
        if (element == nullptr) {
            return false;
        }
        if (this->currentPage == Page::main) {
            const MainPageViewModel::TapAction action = this->mainPageViewModel.HandleTap(*element);
            if (action == MainPageViewModel::TapAction::navigateToSettings) {
                this->outgoingPage = Page::main;
                this->currentPage = Page::settings;
                this->mainPageViewModel.Root().SetVisibility(xaml::attr::Visibility::collapsed);
                this->settingsPageViewModel.Root().SetVisibility(xaml::attr::Visibility::visible);
                this->isTransitioning = xaml::AnimationController::IsAnimating(this->mainPageViewModel.Root())
                    || xaml::AnimationController::IsAnimating(this->settingsPageViewModel.Root());
                return true;
            }
            return action == MainPageViewModel::TapAction::contentChanged;
        }

        if (this->settingsPageViewModel.HandleTap(*element)
            == SettingsPageViewModel::TapAction::navigateToMain) {
            this->outgoingPage = Page::settings;
            this->currentPage = Page::main;
            this->settingsPageViewModel.Root().SetVisibility(xaml::attr::Visibility::collapsed);
            this->mainPageViewModel.Root().SetVisibility(xaml::attr::Visibility::visible);
            this->isTransitioning = xaml::AnimationController::IsAnimating(this->mainPageViewModel.Root())
                || xaml::AnimationController::IsAnimating(this->settingsPageViewModel.Root());
            return true;
        }
        return false;
    }

    void PageManager::CancelTouch() {
        this->touchHandler.CancelTouch();
    }

    void PageManager::UpdateClock() {
        this->animations.Update();
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