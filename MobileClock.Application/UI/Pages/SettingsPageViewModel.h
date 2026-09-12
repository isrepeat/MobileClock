#pragma once
#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeTreeBuilder.h>
#endif
#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Binding.h>

#include "UI/ApplicationActions.h"
#include "UI/ISerializable.h"
#include "UI/PageRegistry.h"
#include "UI/Navigation.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace xaml {
    class IRenderBackend;
    class RendererRegistry;
}

namespace mobileclock::ui {
    class SettingsPageViewModel final : public ISerializable {
    public:
        inline static constexpr std::string_view PageName = "SettingsPage";

        enum class Property {
            theme,
            sound,
        };

        using PropertyChangedHandler = std::function<void(Property)>;
        using Unsubscribe = std::function<void()>;

        explicit SettingsPageViewModel(PageContext& context);
        ~SettingsPageViewModel() = default;

        SettingsPageViewModel(const SettingsPageViewModel&) = delete;
        SettingsPageViewModel& operator=(const SettingsPageViewModel&) = delete;

        const std::string& Theme() const;
        const std::string& Sound() const;

        void NavigateToMain();
        xaml::Element::Command NavigateToMainCommand() const;
        xaml::Element::Command ShareLogsCommand() const;
        xaml::Element::Command ExportLogsCommand() const;
        void Initialize(xaml::Size availableSize);
        void HandleTap(xaml::Element& element);
        void Update();
        void Render(xaml::IRenderBackend& renderer, const xaml::RendererRegistry& renderers) const;
        xaml::Element& Root();
        Unsubscribe Subscribe(PropertyChangedHandler handler);

#if defined(MOBILECLOCK_XAML_PREVIEWER)
        bool Deserialize(std::string_view json, std::string& error) override;
        xaml::runtime::RuntimeBindingContext RuntimeContext();
        void ReplaceRuntimeTree(xaml::runtime::RuntimeBuildResult result);
#endif

    private:
        std::string theme = "Тёмная";
        std::string sound = "Мелодия по умолчанию";
        std::vector<PropertyChangedHandler> propertyChangedHandlers;
        std::unique_ptr<xaml::Element> page;
        xaml::BindingScope bindings;
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        std::unique_ptr<xaml::BindingScope> runtimeBindings;
#endif
        xaml::Element::Command navigateToMainCommand;
        xaml::Element::Command shareLogsCommand;
        xaml::Element::Command exportLogsCommand;
    };
}