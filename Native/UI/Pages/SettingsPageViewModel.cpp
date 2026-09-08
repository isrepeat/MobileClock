#include <XamlRuntime/RenderEngine.h>

#include "!Generated/Xaml/Pages/SettingsPage.xaml.h"
#include "UI/Pages/SettingsPageViewModel.h"

#include <utility>

namespace mobileclock::ui {
    SettingsPageViewModel::SettingsPageViewModel(
        IPageNavigator& navigator,
        IApplicationActions& actions)
        : navigateToMainCommand([&navigator]() {
            navigator.Navigate(Page::main);
        })
        , shareLogsCommand([&actions]() {
            actions.ShareLogs();
        })
        , exportLogsCommand([&actions]() {
            actions.ExportLogs();
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