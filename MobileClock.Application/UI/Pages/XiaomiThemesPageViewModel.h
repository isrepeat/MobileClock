#pragma once
#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeTreeBuilder.h>
#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Binding.h>

#include "UI/Pages/AlarmMelodyViewModel.h"
#include "UI/ISerializable.h"
#include "UI/PageRegistry.h"

#include <optional>
#include <string>
#include <vector>
#include <memory>

namespace xaml {
    class IRenderBackend;
    class RendererRegistry;
}

namespace mobileclock::ui {
    class XiaomiThemesPageViewModel final : public ISerializable, public INavigationPage {
    public:
        inline static constexpr std::string_view PageName = "XiaomiThemesPage";
        inline static constexpr std::string_view PreviewGraphTitle = "Xiaomi Themes";

        explicit XiaomiThemesPageViewModel(PageContext& context);
        ~XiaomiThemesPageViewModel() override = default;

        //
        // ISerializable
        //
        bool Deserialize(std::string_view json, std::string& error) override;

        //
        // INavigationPage
        //
        std::unique_ptr<NavigationState> OnNavigatingFrom(const NavigationRequest& request) override;
        bool OnNavigatingTo(const NavigationRequest& request, std::unique_ptr<NavigationState> state) override;

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
        PageContext& context;
        std::vector<AlarmMelodyViewModel> melodies;
        std::optional<size_t> selectedMelody;
        std::unique_ptr<xaml::Element> page;
        xaml::BindingScope bindings;
        std::unique_ptr<xaml::BindingScope> runtimeBindings;
    };
}
#endif