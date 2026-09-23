#pragma once
#if defined(ANDROID_APP_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/IRuntimeReloadableControl.h>
#endif
#include <XamlRuntime/UserControl.h>

#include "../Interface/IGestureTarget.h"

#include <string_view>
#include <string>
#include <memory>

namespace mobileclock::ui::base {
    class ScrollableListBase : public xaml::UserControl, public interface::IGestureTarget
#if defined(ANDROID_APP_PREVIEWER)
        , public xaml::runtime::IRuntimeReloadableControl
#endif
    {
    public:
        ScrollableListBase() = default;
        ~ScrollableListBase() override = default;

        //
        // IGestureTarget
        //
        xaml::Element* FindScrollViewer(const xaml::Element& element) const override;
        void UpdateGestures(xaml::Element& pageRoot, xaml::AnimationController& animations) override;

#if defined(ANDROID_APP_PREVIEWER)
        //
        // IRuntimeReloadableControl
        //
        bool ReplaceTemplate(const xaml::runtime::XamlElementNode& templateNode,
            const xaml::runtime::RuntimeBindingContext& context, std::string& diagnostics) override;
#endif

    protected:
        virtual std::string_view ScrollViewerId() const = 0;
#if defined(ANDROID_APP_PREVIEWER)
        virtual void preview_OnTemplateReplaced();
#endif
        xaml::Element* FindElement(std::string_view id) const;
        bool Owns(const xaml::Element& element) const override;

    private:
        void OnInitialized() override;
        bool IsIn(const xaml::Element& pageRoot) const override;
    };
}