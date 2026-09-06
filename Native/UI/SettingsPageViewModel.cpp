#include <XamlRuntime/RenderEngine.h>

#include "!Generated/Xaml/SettingsPage.xaml.h"
#include "UI/SettingsPageViewModel.h"

#include <utility>

namespace mobileclock::ui {
    //
    // API
    //
    const std::string& SettingsPageViewModel::Theme() const {
        return this->theme;
    }

    const std::string& SettingsPageViewModel::Sound() const {
        return this->sound;
    }

    void SettingsPageViewModel::BindCommand(std::string name, CommandBindings::Handler handler) {
        this->commands.Bind(std::move(name), std::move(handler));
    }

    void SettingsPageViewModel::Initialize(xaml::Size availableSize) {
        this->bindings.Clear();
        this->page = xaml::generated::SettingsPage::Create(*this, this->bindings);
        xaml::layout(*this->page, availableSize);
    }

    SettingsPageViewModel::TapAction SettingsPageViewModel::HandleTap(xaml::Element& element) {
        if (element.Command() == "navigateToMain") {
            return TapAction::navigateToMain;
        }
        this->bindings.UpdateSource(element);
        this->commands.Execute(element.Command());
        return TapAction::none;
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
}