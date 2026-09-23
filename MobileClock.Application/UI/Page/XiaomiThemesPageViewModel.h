#pragma once
#if defined(ANDROID_APP_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeTreeBuilder.h>
#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Binding.h>

#include "../ViewModel/AlarmMelodyViewModel.h"
#include "../../Interface/INavigationPage.h"
#include "../../Interface/ISerializable.h"
#include "../../Core/PageRegistry.h"

#include <optional>
#include <string>
#include <vector>
#include <memory>

namespace xaml {
    class IRenderBackend;
    class RendererRegistry;
}

namespace mobileclock::application::ui::page {
    class preview_XiaomiThemesPageViewModel final : public interface::ISerializable, public interface::INavigationPage {
    public:
        inline static constexpr std::string_view PageName = "XiaomiThemesPage";
        inline static constexpr std::string_view preview_GraphTitle = "Xiaomi Themes";

        explicit preview_XiaomiThemesPageViewModel(core::PageContext& context);
        ~preview_XiaomiThemesPageViewModel() override = default;

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

        void preview_Initialize(xaml::Size availableSize);
        void preview_HandleTap(xaml::Element& element);
        void preview_Update();
        void preview_Render(xaml::IRenderBackend& renderer, const xaml::RendererRegistry& renderers) const;
        xaml::Element& preview_Root();
        xaml::runtime::RuntimeBindingContext preview_RuntimeContext();
        void preview_ReplaceRuntimeTree(xaml::runtime::RuntimeBuildResult result);

    private:
        void preview_ApplySelectedMelody();
        void preview_ConnectControls();
        void preview_RebuildMelodies();
        void preview_Refresh();
        xaml::Element* preview_Find(std::string_view id) const;

    private:
        core::PageContext& context;
        std::vector<view_model::AlarmMelodyViewModel> melodies;
        std::optional<size_t> selectedMelody;
        std::unique_ptr<xaml::Element> page;
        xaml::BindingScope bindings;
        std::unique_ptr<xaml::BindingScope> runtimeBindings;
    };
}
#endif