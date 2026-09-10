#include "UI/Pages/SettingsPageViewModel.h"

#include <XamlRuntime/RenderEngine.h>

#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <JsonParser/JsonParser.h>
#endif

#include "!Generated/MobileClock.Application/Xaml/Pages/SettingsPage.xaml.h"
#include "UI/Pages/MainPageViewModel.h"

#include <utility>
#include <format>

namespace mobileclock::ui {
#if defined(MOBILECLOCK_XAML_PREVIEWER)
    namespace _details {
        struct SettingsPagePreviewScenario final {
            std::optional<std::string> Theme = "Тёмная";
            std::optional<std::string> Sound = "Мелодия по умолчанию";

            JS_OBJECT(JS_MEMBER(Theme), JS_MEMBER(Sound));
        };
    }
#endif

    SettingsPageViewModel::SettingsPageViewModel(PageContext& context)
        : navigateToMainCommand([&context]() {
            context.navigator.Navigate<MainPageViewModel>();
        })
        , shareLogsCommand([&context]() {
            context.actions.ShareLogs();
        })
        , exportLogsCommand([&context]() {
            context.actions.ExportLogs();
        }) {
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

    xaml::Element::Command SettingsPageViewModel::NavigateToMainCommand() const {
        return this->navigateToMainCommand;
    }

    xaml::Element::Command SettingsPageViewModel::ShareLogsCommand() const {
        return this->shareLogsCommand;
    }

    xaml::Element::Command SettingsPageViewModel::ExportLogsCommand() const {
        return this->exportLogsCommand;
    }

    void SettingsPageViewModel::Initialize(xaml::Size availableSize) {
        this->bindings.Clear();
        this->page = xaml::generated::SettingsPage::Create(*this, this->bindings);
        xaml::layout(*this->page, availableSize);
    }

    void SettingsPageViewModel::HandleTap(xaml::Element& element) {
        this->bindings.UpdateSource(element);
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
    bool SettingsPageViewModel::Deserialize(std::string_view json, std::string& error) {
        _details::SettingsPagePreviewScenario scenario;
        JS::ParseContext context(json.data(), json.size());
        if (context.parseTo(scenario) != JS::Error::NoError) {
            error = std::format("Invalid preview scenario JSON: {}", context.makeErrorString());
            return false;
        }
        if (scenario.Theme) {
            this->theme = std::move(*scenario.Theme);
        }
        if (scenario.Sound) {
            this->sound = std::move(*scenario.Sound);
        }
        return true;
    }
#endif
}