#pragma once
#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/IRuntimeReloadableControl.h>
#endif
#include <XamlRuntime/ScrollController.h>
#include <XamlRuntime/UserControl.h>

#include "MobileClock.UI/Controls/IGestureTarget.h"

#include <memory>
#include <string>
#include <string_view>

namespace mobileclock::ui::controls {
    class PannableList : public xaml::UserControl, public IGestureTarget
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        , public xaml::runtime::IRuntimeReloadableControl
#endif
    {
    public:
        PannableList() = default;
        ~PannableList() override = default;

        bool CanHandlePan(const xaml::Element& element) const override;
        bool IsVerticalPan() const override;
        xaml::Element* FindScrollViewer(const xaml::Element& element) const override;
        void BeginPan(const PanState& state) override;
        void UpdatePan(const PanState& state) override;
        bool EndPan(const PanState& state, xaml::AnimationController& animations) override;
        void CancelPan(xaml::Element& element) override;
        void UpdateGestures(xaml::Element& pageRoot, xaml::AnimationController& animations) override;

#if defined(MOBILECLOCK_XAML_PREVIEWER)
        bool ReplaceTemplate(const xaml::runtime::XamlElementNode& templateNode,
            const xaml::runtime::RuntimeBindingContext& context, std::string& diagnostics) override;
#endif

    protected:
        virtual std::string_view ScrollViewerId() const = 0;
        xaml::Element* FindElement(std::string_view id) const;
        bool Owns(const xaml::Element& element) const override;

    private:
        void OnInitialized() override;
        bool IsIn(const xaml::Element& pageRoot) const override;

    private:
        xaml::ScrollController scrollController;
        float lastPanY = 0.0f;
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        std::unique_ptr<xaml::BindingScope> runtimeBindings;
#endif
    };
}