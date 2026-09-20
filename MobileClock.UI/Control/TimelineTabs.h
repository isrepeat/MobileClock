#pragma once
#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/IRuntimeReloadableControl.h>
#endif
#include "!Generated/MobileClock.UI/Xaml/Control/TimelineTabs.xaml.h"

#include <XamlRuntime/UserControl.h>

namespace mobileclock::ui::control {
    class TimelineTabs final : public xaml::UserControl
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        , public xaml::runtime::IRuntimeReloadableControl
#endif
    {
    public:
        TimelineTabs() = default;
        ~TimelineTabs() override = default;

#if defined(MOBILECLOCK_XAML_PREVIEWER)
        //
        // IRuntimeReloadableControl
        //
        std::string_view RuntimeClassName() const override;
        bool ReplaceTemplate(const xaml::runtime::XamlElementNode& templateNode,
            const xaml::runtime::RuntimeBindingContext& context, std::string& diagnostics) override;
#endif

        template <typename TViewModel>
        static std::unique_ptr<TimelineTabs> Create(TViewModel& viewModel, xaml::BindingScope&) {
            auto control = std::make_unique<TimelineTabs>();
            auto bindings = std::make_unique<xaml::BindingScope>();
            auto content = xaml::generated::TimelineTabsXaml::BuildContent(viewModel, *bindings);
            control->InitializeComponent(
                std::move(content), std::move(bindings));
            return control;
        }

    private:
        void OnInitialized() override;

    };
}