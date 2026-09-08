#pragma once

#include "!Generated/Xaml/Controls/TimelineTabs.xaml.h"

namespace mobileclock::ui::controls {
    class TimelineTabs final {
    public:
        template <typename TViewModel>
        static std::unique_ptr<xaml::Element> Create(TViewModel& viewModel, xaml::BindingScope& bindings) {
            return xaml::generated::TimelineTabsXaml::Create(viewModel, bindings);
        }
    };
}