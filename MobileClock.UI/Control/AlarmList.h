#pragma once
#if defined(ANDROID_APP_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/IRuntimeReloadableControl.h>
#endif
#include <XamlRuntime/DependentProperty.h>
#include <XamlRuntime/XamlLayout.h>

#include "!Generated/MobileClock.UI/Xaml/Control/AlarmList.xaml.h"
#include "../Base/InteractiveListBase.h"

#include <functional>
#include <chrono>
#include <vector>
#include <memory>

namespace mobileclock::ui::control {
    class AlarmList final : public base::InteractiveListBase {
    public:
        AlarmList() = default;
        ~AlarmList() override = default;

#if defined(ANDROID_APP_PREVIEWER)
        //
        // IRuntimeReloadableControl
        //
        std::string_view RuntimeClassName() const override;
        bool ReplaceTemplate(const xaml::runtime::XamlElementNode& templateNode,
            const xaml::runtime::RuntimeBindingContext& context, std::string& diagnostics) override;
        static void preview_PreserveInstances(xaml::Element& previous, xaml::Element& replacement, xaml::BindingScope& bindings);
#endif

        template <typename TViewModel, typename TItemsSource>
        static std::unique_ptr<AlarmList> Create(
            TViewModel& viewModel,
            const TItemsSource& itemsSource,
            xaml::BindingScope&) {
            auto control = std::make_unique<AlarmList>();
            control->SetItemsSource(itemsSource);
            control->SetRemoveHandler([&viewModel](const void* dataContext) { return viewModel.RemoveItem(dataContext); });
            auto bindings = std::make_unique<xaml::BindingScope>();
            auto content = xaml::generated::AlarmListXaml::BuildContent(viewModel, itemsSource, *bindings);
            control->InitializeComponent(
                std::move(content), std::move(bindings));
            return control;
        }

        const xaml::DependentProperty<const void*>& ItemsSourceProperty() const;

    private:
        std::string_view ScrollViewerId() const override;
        std::string_view ListViewId() const override;

        //
        // InteractiveListBase
        //
        interface::GestureHandling ResolveInteractiveGesture(
            const interface::IGestureTarget::PanState& state,
            interface::GestureDirection direction) const override;
        void BeginInteractiveGesture(const interface::IGestureTarget::PanState& state) override;
        void UpdateInteractiveGesture(const interface::IGestureTarget::PanState& state) override;
        bool EndInteractiveGesture(const interface::IGestureTarget::PanState& state, xaml::AnimationController& animations) override;
        void CancelInteractiveGesture(xaml::Element& element) override;
        template <typename TItemsSource>
        void SetItemsSource(const TItemsSource& value) {
            this->itemsSource.Set(static_cast<const void*>(&value));
        }
        xaml::DependentProperty<const void*> itemsSource;
    };
}