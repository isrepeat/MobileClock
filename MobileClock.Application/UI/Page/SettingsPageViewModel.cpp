#include "SettingsPageViewModel.h"

#if defined(MOBILECLOCK_XAML_PREVIEWER)
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

    SettingsPageViewModel::SettingsPageViewModel(core::PageContext& context)
        : navigateToMainCommand([&context]() {
            context.navigator.Trigger(core::NavigationTrigger::navigateBack);
        })
        , resetAlarmMelodySelectionCommand([&context]() {
            context.appSessionController.Dispatch(core::AppSessionSignal::resetAlarmMelodySelection, {});
        })
        , shareLogsCommand([&context]() {
            context.appSessionController.Dispatch(core::AppSessionSignal::shareLogs, {});
        })
        , exportLogsCommand([&context]() {
            context.appSessionController.Dispatch(core::AppSessionSignal::exportLogs, {});
        }) {
    }

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
        JS::ParseContext context(json.data(), json.size());
        if (context.parseTo(scenario) != JS::Error::NoError) {
            error = std::format("Invalid serialized JSON: {}", context.makeErrorString());
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
        this->bindings.Clear();
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        this->runtimeBindings.reset();
#endif
        this->page = xaml::generated::SettingsPage::Create(*this, this->bindings);
        xaml::layout(*this->page, availableSize);
    }

    void SettingsPageViewModel::HandleTap(xaml::Element& element) {
        this->bindings.UpdateSource(element);
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        if (this->runtimeBindings) {
            this->runtimeBindings->UpdateSource(element);
        }
#endif
        element.ExecuteCommand();
    }

    void SettingsPageViewModel::Update() {
    }

    void SettingsPageViewModel::Render(
        xaml::IRenderBackend& renderer,
        const xaml::RendererRegistry& renderers) const {
        xaml::Render(*this->page, renderer, renderers);
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

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    xaml::runtime::RuntimeBindingContext SettingsPageViewModel::RuntimeContext() {
        auto registry = std::make_shared<xaml::runtime::RuntimeBindingRegistry>();
        xaml::runtime::RuntimeBindingPublisher publisher{*registry, *this};
        publisher.Text("Theme", Property::theme, &SettingsPageViewModel::Theme);
        publisher.Text("Sound", Property::sound, &SettingsPageViewModel::Sound);
        publisher.Command("NavigateToMainCommand", &SettingsPageViewModel::NavigateToMainCommand);
        publisher.Command("ResetAlarmMelodySelectionCommand", &SettingsPageViewModel::ResetAlarmMelodySelectionCommand);
        publisher.Command("ShareLogsCommand", &SettingsPageViewModel::ShareLogsCommand);
        publisher.Command("ExportLogsCommand", &SettingsPageViewModel::ExportLogsCommand);
        xaml::runtime::RuntimeBindingContext result{registry, "SettingsPageViewModel", {}};
        result.xamlNamespace = "urn:mobileclock:xaml";
        result.controlXmlNamespace = "using:mobileclock.ui.control";

        return result;
    }

    void SettingsPageViewModel::ReplaceRuntimeTree(xaml::runtime::RuntimeBuildResult result) {
        this->bindings.Clear();
        this->runtimeBindings.reset();
        this->page = std::move(result.root);
        this->runtimeBindings = std::move(result.bindings);
    }
#endif
}