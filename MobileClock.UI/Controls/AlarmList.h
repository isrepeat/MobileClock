#pragma once
#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/IRuntimeReloadableControl.h>
#endif
#include <XamlRuntime/DependentProperty.h>
#include <XamlRuntime/XamlLayout.h>

#include "!Generated/MobileClock.UI/Xaml/Controls/AlarmList.xaml.h"
#include "MobileClock.UI/Controls/InteractiveList.h"

#include <chrono>
#include <functional>
#include <memory>
#include <vector>

namespace mobileclock::ui::controls {
    class AlarmList final : public InteractiveList {
    public:
        AlarmList() = default;
        ~AlarmList() override = default;

#if defined(MOBILECLOCK_XAML_PREVIEWER)
        //
        // IRuntimeReloadableControl
        //
        std::string_view RuntimeClassName() const override;
        bool ReplaceTemplate(const xaml::runtime::XamlElementNode& templateNode,
            const xaml::runtime::RuntimeBindingContext& context, std::string& diagnostics) override;
        static void PreserveInstances(xaml::Element& previous, xaml::Element& replacement, xaml::BindingScope& bindings);
#endif

        template <typename TViewModel, typename TItemsSource>
        static std::unique_ptr<AlarmList> Create(
            TViewModel& viewModel,
            const TItemsSource& itemsSource,
            xaml::BindingScope& bindings) {
            auto control = std::make_unique<AlarmList>();
            control->SetItemsSource(itemsSource);
            control->SetRemoveHandler([&viewModel](const void* dataContext) { return viewModel.RemoveItem(dataContext); });
            control->InitializeComponent(
                xaml::generated::AlarmListXaml::BuildContent(viewModel, itemsSource, bindings));
            return control;
        }

        const xaml::DependentProperty<const void*>& ItemsSourceProperty() const;

    private:
        std::string_view ScrollViewerId() const override;
        std::string_view ListViewId() const override;

        //
        // InteractiveList
        //
        GestureHandling ResolveInteractiveGesture(
            const PanState& state,
            GestureDirection direction) const override;
        void BeginInteractiveGesture(const PanState& state) override;
        void UpdateInteractiveGesture(const PanState& state) override;
        bool EndInteractiveGesture(const PanState& state, xaml::AnimationController& animations) override;
        void CancelInteractiveGesture(xaml::Element& element) override;
        template <typename TItemsSource>
        void SetItemsSource(const TItemsSource& value) {
            this->itemsSource.Set(static_cast<const void*>(&value));
        }
        xaml::DependentProperty<const void*> itemsSource;
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        std::unique_ptr<xaml::BindingScope> runtimeBindings;
#endif
    };
}