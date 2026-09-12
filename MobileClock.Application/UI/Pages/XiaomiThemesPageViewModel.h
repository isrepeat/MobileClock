#pragma once
#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeTreeBuilder.h>
#endif
#include <XamlRuntime/Binding.h>
#include <XamlRuntime/XamlLayout.h>

#include "UI/ISerializable.h"
#include "UI/PageRegistry.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace xaml {
    class IRenderBackend;
    class RendererRegistry;
}

namespace mobileclock::ui {
    class XiaomiThemesPageViewModel final : public ISerializable {
    public:
        inline static constexpr std::string_view PageName = "XiaomiThemesPage";

        class Melody final {
        public:
            Melody(std::string name, std::string uri);

            const std::string& Name() const;
            const std::string& Uri() const;

        private:
            std::string name;
            std::string uri;
        };

        explicit XiaomiThemesPageViewModel(PageContext& context);
        ~XiaomiThemesPageViewModel() override = default;

#if defined(MOBILECLOCK_XAML_PREVIEWER)
        //
        // ISerializable
        //
        bool Deserialize(std::string_view json, std::string& error) override;
#endif

        void ApplySelectedMelody();
        void Initialize(xaml::Size availableSize);
        void HandleTap(xaml::Element& element);
        void Update();
        void Render(xaml::IRenderBackend& renderer, const xaml::RendererRegistry& renderers) const;
        xaml::Element& Root();
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        xaml::runtime::RuntimeBindingContext RuntimeContext();
        void ReplaceRuntimeTree(xaml::runtime::RuntimeBuildResult result);
#endif

    private:
        void ConnectControls();
        void RebuildMelodies();
        void Refresh();
        xaml::Element* Find(std::string_view id) const;

    private:
        PageContext& context;
        std::vector<Melody> melodies;
        std::optional<size_t> selectedMelody;
        std::unique_ptr<xaml::Element> page;
        xaml::BindingScope bindings;
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        std::unique_ptr<xaml::BindingScope> runtimeBindings;
#endif
    };
}