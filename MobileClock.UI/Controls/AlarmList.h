#pragma once
#include "!Generated/Xaml/Controls/AlarmList.xaml.h"

#include <XamlRuntime/DependentProperty.h>
#include <XamlRuntime/UserControl.h>
#include <XamlRuntime/XamlLayout.h>

#include <memory>

namespace mobileclock::ui::controls {
    class AlarmList final : public xaml::UserControl {
    public:
        AlarmList() = default;
        ~AlarmList() override = default;

        template <typename TViewModel, typename TItemsSource>
        static std::unique_ptr<AlarmList> Create(
            TViewModel& viewModel,
            const TItemsSource& itemsSource,
            xaml::BindingScope& bindings) {
            auto control = std::make_unique<AlarmList>();
            control->SetItemsSource(itemsSource);
            control->InitializeComponent(
                xaml::generated::AlarmListXaml::BuildContent(viewModel, itemsSource, bindings));
            return control;
        }

        const xaml::DependentProperty<const void*>& ItemsSourceProperty() const;

    private:
        template <typename TItemsSource>
        void SetItemsSource(const TItemsSource& value) {
            this->itemsSource.Set(static_cast<const void*>(&value));
        }

        void OnInitialized() override;

    private:
        xaml::DependentProperty<const void*> itemsSource;
    };
}