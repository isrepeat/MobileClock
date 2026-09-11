#pragma once
#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/IRuntimeReloadableControl.h>
#endif
#include <XamlRuntime/UserControl.h>

#include "!Generated/MobileClock.UI/Xaml/Controls/AlarmActionsMenu.xaml.h"
#include "MobileClock.UI/Controls/IGestureTarget.h"

#include <string_view>
#include <memory>

namespace mobileclock::ui::controls {
    class AlarmActionsMenu final : public xaml::UserControl, public IGestureTarget
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        , public xaml::runtime::IRuntimeReloadableControl
#endif
    {
    public:
        AlarmActionsMenu() = default;
        ~AlarmActionsMenu() override = default;

    private:
        //
        // UserControl
        //
        void OnInitialized() override;

    public:
        //
        // IGestureTarget
        //
        bool CanHandlePan(const xaml::Element& element) const override;
        bool IsVerticalPan() const override;
        xaml::Element* FindScrollViewer(const xaml::Element& element) const override;
        void BeginPan(const PanState& state) override;
        void UpdatePan(const PanState& state) override;
        bool EndPan(const PanState& state, xaml::AnimationController& animations) override;
        void CancelPan(xaml::Element& element) override;
        void UpdateGestures(xaml::Element& pageRoot, xaml::AnimationController& animations) override;

    private:
        bool IsIn(const xaml::Element& pageRoot) const override;
        bool Owns(const xaml::Element& element) const override;

    public:
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        //
        // IRuntimeReloadableControl
        //
        std::string_view RuntimeClassName() const override;
        bool ReplaceTemplate(const xaml::runtime::XamlElementNode& templateNode,
            const xaml::runtime::RuntimeBindingContext& context, std::string& diagnostics) override;
#endif

        template<typename TViewModel>
        static std::unique_ptr<AlarmActionsMenu> Create(TViewModel& viewModel, xaml::BindingScope& bindings) {
            auto control = std::make_unique<AlarmActionsMenu>();
            control->InitializeComponent(
                xaml::generated::AlarmActionsMenuXaml::BuildContent(viewModel, bindings));
            return control;
        }

        bool IsExpanded() const;
        void SetIsExpanded(bool value);
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        static void PreserveState(const xaml::Element& previous, xaml::Element& replacement);
#endif

    private:
        xaml::Element* FindElement(std::string_view id) const;
        void ApplyState(bool useTransitions);
        float StateValue(const char* state, const char* target, xaml::AnimatedProperty property) const;
        void SetDragProgress(float progress);

    private:
        bool isExpanded = false;
        float panStartHeight = 0.0f;
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        std::unique_ptr<xaml::BindingScope> runtimeBindings;
#endif
    };
}