#include "SettingsPageViewModel.h"

#if defined(ANDROID_APP_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeBindingPublisher.h>
#endif
#include <XamlRuntime/RenderEngine.h>
#include <JsonParser/json_struct/json_struct.h>

#include "!Generated/MobileClock.Application/Xaml/Page/SettingsPage.xaml.h"
#include "../../Core/AppSessionController.h"
#include "MainPageViewModel.h"

#include <utility>
#include <format>

namespace mobileclock::application::ui::page {
    namespace _details {
        struct SettingsPageSerializationDocument final {
            std::optional<std::string> Theme = "Тёмная";
            std::optional<std::string> Sound = "Мелодия по умолчанию";

            JS_OBJECT(
                JS_MEMBER(Theme),
                JS_MEMBER(Sound)
            );
        };
    } // namespace _details

    SettingsPageViewModel::SettingsPageViewModel(core::PageContext& pageContext)
        : navigateToMainCommand([&pageContext]() {
            pageContext.navigator.Trigger(core::NavigationTrigger::navigateBack);
        })
        , resetAlarmMelodySelectionCommand([&pageContext]() {
            pageContext.appSessionController.Dispatch(core::AppSessionSignal::resetAlarmMelodySelection, {});
        })
        , shareLogsCommand([&pageContext]() {
            pageContext.appSessionController.Dispatch(core::AppSessionSignal::shareLogs, {});
        })
        , exportLogsCommand([&pageContext]() {
            pageContext.appSessionController.Dispatch(core::AppSessionSignal::exportLogs, {});
        }) {
    }

#if defined(ANDROID_APP_PREVIEWER)
    //
    // ISerializable
    //
    std::string SettingsPageViewModel::Serialize() const {
        _details::SettingsPageSerializationDocument scenario;
        scenario.Theme = this->theme;
        scenario.Sound = this->sound;
        return JS::serializeStruct(scenario);
    }

    bool SettingsPageViewModel::Deserialize(std::string_view json, std::string& error) {
        _details::SettingsPageSerializationDocument scenario;
        JS::ParseContext pageContext(json.data(), json.size());
        if (pageContext.parseTo(scenario) != JS::Error::NoError) {
            error = std::format("Invalid serialized JSON: {}", pageContext.makeErrorString());
            return false;
        }
        if (scenario.Theme) {
            this->theme = std::move(*scenario.Theme);
            for (const auto& handler : this->propertyChangedHandlers) {
                if (handler) {
                    handler(Property::theme);
                }
            }
        }
        if (scenario.Sound) {
            this->sound = std::move(*scenario.Sound);
            for (const auto& handler : this->propertyChangedHandlers) {
                if (handler) {
                    handler(Property::sound);
                }
            }
        }
        return true;
    }
#endif


    //
    // INavigationPage
    //
    std::unique_ptr<base::NavigationStateBase> SettingsPageViewModel::OnNavigatingFrom(const core::NavigationRequest&) {
        return {};
    }

    bool SettingsPageViewModel::OnNavigatingTo(const core::NavigationRequest&, std::unique_ptr<base::NavigationStateBase>) {
        return true;
    }

    //
    // API
    //
    const std::string& SettingsPageViewModel::Theme() const {
        return this->theme;
    }

    const std::string& SettingsPageViewModel::Sound() const {
        return this->sound;
    }

    void SettingsPageViewModel::NavigateToMain() {
        this->navigateToMainCommand();
    }

    xaml::Element::Command SettingsPageViewModel::NavigateToMainCommand() const {
        return this->navigateToMainCommand;
    }

    xaml::Element::Command SettingsPageViewModel::ResetAlarmMelodySelectionCommand() const {
        return this->resetAlarmMelodySelectionCommand;
    }

    xaml::Element::Command SettingsPageViewModel::ShareLogsCommand() const {
        return this->shareLogsCommand;
    }

    xaml::Element::Command SettingsPageViewModel::ExportLogsCommand() const {
        return this->exportLogsCommand;
    }

    void SettingsPageViewModel::Initialize(xaml::Size availableSize) {
        this->bindingScope.Clear();
#if defined(ANDROID_APP_PREVIEWER)
        this->runtimeBindingScope.reset();
#endif
        this->page = xaml::generated::SettingsPage::Create(*this, this->bindingScope);
        xaml::layout(*this->page, availableSize);
    }

    void SettingsPageViewModel::HandleTap(xaml::Element& element) {
        this->bindingScope.UpdateSource(element);
#if defined(ANDROID_APP_PREVIEWER)
        if (this->runtimeBindingScope) {
            this->runtimeBindingScope->UpdateSource(element);
        }
#endif
        element.ExecuteCommand();
    }

    void SettingsPageViewModel::Update() {
    }

    void SettingsPageViewModel::Render(
        xaml::IRenderBackend& renderer,
        const xaml::RendererRegistry& rendererRegistry) const {
        xaml::Render(*this->page, renderer, rendererRegistry);
    }

    xaml::Element& SettingsPageViewModel::Root() {
        return *this->page;
    }

    SettingsPageViewModel::Unsubscribe SettingsPageViewModel::Subscribe(PropertyChangedHandler handler) {
        this->propertyChangedHandlers.push_back(std::move(handler));
        const size_t index = this->propertyChangedHandlers.size() - 1;
        return [this, index]() {
            this->propertyChangedHandlers[index] = nullptr;
        };
    }

#if defined(ANDROID_APP_PREVIEWER)
    xaml::runtime::RuntimeBindingContext SettingsPageViewModel::preview_RuntimeContext() {
        auto registry = std::make_shared<xaml::runtime::RuntimeBindingRegistry>();
        xaml::runtime::RuntimeBindingPublisher publisher{*registry, *this};
        publisher.Text("Theme", Property::theme, &SettingsPageViewModel::Theme);
        publisher.Text("Sound", Property::sound, &SettingsPageViewModel::Sound);
        publisher.Command("NavigateToMainCommand", &SettingsPageViewModel::NavigateToMainCommand);
        publisher.Command("ResetAlarmMelodySelectionCommand", &SettingsPageViewModel::ResetAlarmMelodySelectionCommand);
        publisher.Command("ShareLogsCommand", &SettingsPageViewModel::ShareLogsCommand);
        publisher.Command("ExportLogsCommand", &SettingsPageViewModel::ExportLogsCommand);
        xaml::runtime::RuntimeBindingContext runtimeBindingContext{registry, "SettingsPageViewModel", {}};
        runtimeBindingContext.xamlNamespace = "urn:mobileclock:xaml";
        runtimeBindingContext.controlXmlNamespace = "using:mobileclock.ui.control";

        return runtimeBindingContext;
    }

    void SettingsPageViewModel::preview_ReplaceRuntimeTree(xaml::runtime::RuntimeBuildResult runtimeBuildResult) {
        this->bindingScope.Clear();
        this->runtimeBindingScope.reset();
        this->page = std::move(runtimeBuildResult.root);
        this->runtimeBindingScope = std::move(runtimeBuildResult.bindings);
    }
#endif
}