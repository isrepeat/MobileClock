#pragma once
#include "!Generated/Xaml/Controls/TimelineTabs.xaml.h"

#include <XamlRuntime/UserControl.h>

namespace mobileclock::ui::controls {
    class TimelineTabs final : public xaml::UserControl {
    public:
        TimelineTabs() = default;
        ~TimelineTabs() override = default;

        template <typename TViewModel>
        static std::unique_ptr<TimelineTabs> Create(TViewModel& viewModel, xaml::BindingScope& bindings) {
            auto control = std::make_unique<TimelineTabs>();
            control->InitializeComponent(
                xaml::generated::TimelineTabsXaml::BuildContent(viewModel, bindings));
            return control;
        }

    private:
        void OnInitialized() override;
    };
}