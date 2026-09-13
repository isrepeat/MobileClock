#pragma once
#include <XamlRuntime/DependentProperty.h>

#include "!Generated/MobileClock.UI/Xaml/Controls/ScrollableList.xaml.h"
#include "MobileClock.UI/Controls/PannableList.h"

#include <memory>
#include <string_view>

namespace mobileclock::ui::controls {
    class ScrollableList final : public PannableList {
    public:
        ScrollableList() = default;
        ~ScrollableList() override = default;

        template <typename TViewModel>
        static std::unique_ptr<ScrollableList> Create(
            TViewModel& viewModel,
            xaml::BindingScope& bindings) {
            return Create(viewModel, viewModel.Melodies(), bindings);
        }

        template <typename TViewModel, typename TItemsSource>
        static std::unique_ptr<ScrollableList> Create(
            TViewModel& viewModel,
            const TItemsSource& itemsSource,
            xaml::BindingScope& bindings) {
            auto control = std::make_unique<ScrollableList>();
            control->itemsSource.Set(static_cast<const void*>(&itemsSource));
            control->InitializeComponent(
                xaml::generated::ScrollableListXaml::BuildContent(viewModel, itemsSource, bindings));
            return control;
        }

        const xaml::DependentProperty<const void*>& ItemsSourceProperty() const;

#if defined(MOBILECLOCK_XAML_PREVIEWER)
        //
        // IRuntimeReloadableControl
        //
        std::string_view RuntimeClassName() const override;
#endif

    private:
        std::string_view ScrollViewerId() const override;

    private:
        xaml::DependentProperty<const void*> itemsSource;
    };
}