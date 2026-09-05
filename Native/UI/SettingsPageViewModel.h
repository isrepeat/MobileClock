#pragma once

#include "UI/CommandBindings.h"
#include "XamlRuntime/Binding.h"
#include "XamlRuntime/XamlLayout.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace xaml {
    class AnimationController;
    class IRenderBackend;
    class RendererRegistry;
}

namespace mobileclock::ui {
    class SettingsPageViewModel final {
    public:
        enum class Property {
            theme,
            sound,
        };

        enum class TouchAction {
            none,
            navigateToMain,
        };

        using PropertyChangedHandler = std::function<void(Property)>;
        using Unsubscribe = std::function<void()>;

        SettingsPageViewModel() = default;
        ~SettingsPageViewModel() = default;

        SettingsPageViewModel(const SettingsPageViewModel&) = delete;
        SettingsPageViewModel& operator=(const SettingsPageViewModel&) = delete;

        const std::string& Theme() const;
        const std::string& Sound() const;

        void BindCommand(std::string name, CommandBindings::Handler handler);
        void Initialize(xaml::Size availableSize);
        void HandleTouchDown(float x, float y);
        TouchAction HandleTouchUp(float x, float y, xaml::AnimationController& animations);
        void CancelTouch();
        void Render(xaml::IRenderBackend& renderer, const xaml::RendererRegistry& renderers) const;
        xaml::Element& Root();
        Unsubscribe Subscribe(PropertyChangedHandler handler);

    private:
        std::string theme = "Тёмная";
        std::string sound = "Мелодия по умолчанию";
        std::vector<PropertyChangedHandler> propertyChangedHandlers;
        std::unique_ptr<xaml::Element> page;
        xaml::BindingScope bindings;
        CommandBindings commands;
        xaml::Element* capturedElement = nullptr;
    };
}