#pragma once

#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Binding.h>

#include "UI/CommandBindings.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace xaml {
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

        enum class TapAction {
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
        TapAction HandleTap(xaml::Element& element);
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
    };
}