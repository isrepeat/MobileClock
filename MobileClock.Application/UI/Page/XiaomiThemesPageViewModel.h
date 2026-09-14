#pragma once
#if defined(MOBILECLOCK_XAML_PREVIEWER)
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

    class XiaomiThemesPageViewModel final : public interface::ISerializable, public interface::INavigationPage {
    public:
        inline static constexpr std::string_view PageName = "XiaomiThemesPage";
        inline static constexpr std::string_view PreviewGraphTitle = "Xiaomi Themes";

        explicit XiaomiThemesPageViewModel(core::PageContext& context);
        ~XiaomiThemesPageViewModel() override = default;

        //
        // ISerializable
        //
        bool Deserialize(std::string_view json, std::string& error) override;

        //
        // INavigationPage
        //
        std::unique_ptr<base::NavigationStateBase> OnNavigatingFrom(const core::NavigationRequest& request) override;
        bool OnNavigatingTo(const core::NavigationRequest& request, std::unique_ptr<base::NavigationStateBase> state) override;

        void Initialize(xaml::Size availableSize);
        void HandleTap(xaml::Element& element);
        void Update();
        void Render(xaml::IRenderBackend& renderer, const xaml::RendererRegistry& renderers) const;
        xaml::Element& Root();
        xaml::runtime::RuntimeBindingContext RuntimeContext();
        void ReplaceRuntimeTree(xaml::runtime::RuntimeBuildResult result);

    private:
        void ConnectControls();
        void RebuildMelodies();
        void Refresh();
        xaml::Element* Find(std::string_view id) const;

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