#pragma once
#if defined(ANDROID_APP_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeTreeBuilder.h>
#endif
#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Binding.h>

#include "../../Interface/INavigationPage.h"
#include "../../Interface/ISerializable.h"
#include "../../Core/PageRegistry.h"
#include "../../Core/Navigation.h"

#include <functional>
#include <string>
#include <vector>
#include <memory>

namespace xaml {
    class IRenderBackend;
    class RendererRegistry;
}

namespace mobileclock::application::ui::page {

    class SettingsPageViewModel final : public interface::ISerializable, public interface::INavigationPage {
    public:
        inline static constexpr std::string_view PageName = "SettingsPage";
        inline static constexpr std::string_view preview_GraphTitle = "⚙  Настройки";

        enum class Property {
            theme,
            sound,
        };

        using PropertyChangedHandler = std::function<void(Property)>;
        using Unsubscribe = std::function<void()>;

        explicit SettingsPageViewModel(core::PageContext& context);
        ~SettingsPageViewModel() = default;

        SettingsPageViewModel(const SettingsPageViewModel&) = delete;
        SettingsPageViewModel& operator=(const SettingsPageViewModel&) = delete;

        //
        // ISerializable
        //
        std::string Serialize() const override;
        bool Deserialize(std::string_view json, std::string& error) override;

        //
        // INavigationPage
        //
        std::unique_ptr<base::NavigationStateBase> OnNavigatingFrom(const core::NavigationRequest& request) override;
        bool OnNavigatingTo(const core::NavigationRequest& request, std::unique_ptr<base::NavigationStateBase> state) override;

        const std::string& Theme() const;
        const std::string& Sound() const;

        void NavigateToMain();
        xaml::Element::Command NavigateToMainCommand() const;
        xaml::Element::Command ResetAlarmMelodySelectionCommand() const;
        xaml::Element::Command ShareLogsCommand() const;
        xaml::Element::Command ExportLogsCommand() const;
        void Initialize(xaml::Size availableSize);
        void HandleTap(xaml::Element& element);
        void Update();
        void Render(xaml::IRenderBackend& renderer, const xaml::RendererRegistry& renderers) const;
        xaml::Element& Root();
        Unsubscribe Subscribe(PropertyChangedHandler handler);

#if defined(ANDROID_APP_PREVIEWER)
        xaml::runtime::RuntimeBindingContext preview_RuntimeContext();
        void preview_ReplaceRuntimeTree(xaml::runtime::RuntimeBuildResult result);
#endif

    private:
        std::string theme = "Тёмная";
        std::string sound = "Мелодия по умолчанию";
        std::vector<PropertyChangedHandler> propertyChangedHandlers;
        std::unique_ptr<xaml::Element> page;
        xaml::BindingScope bindings;
#if defined(ANDROID_APP_PREVIEWER)
        std::unique_ptr<xaml::BindingScope> runtimeBindings;
#endif
        xaml::Element::Command navigateToMainCommand;
        xaml::Element::Command resetAlarmMelodySelectionCommand;
        xaml::Element::Command shareLogsCommand;
        xaml::Element::Command exportLogsCommand;
    };
}