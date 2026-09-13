#pragma once
#include <XamlRuntime/DependentProperty.h>

#include "!Generated/MobileClock.UI/Xaml/Controls/AlarmMelodyList.xaml.h"
#include "MobileClock.UI/Controls/InteractiveList.h"

#include <memory>
#include <string_view>

namespace mobileclock::ui::controls {
    class AlarmMelodyList final : public InteractiveList {
    public:
        AlarmMelodyList() = default;
        ~AlarmMelodyList() override = default;

        template <typename TViewModel>
        static std::unique_ptr<AlarmMelodyList> Create(
            TViewModel& viewModel,
            xaml::BindingScope& bindings) {
            return Create(viewModel, viewModel.Melodies(), bindings);
        }

        template <typename TViewModel, typename TItemsSource>
        static std::unique_ptr<AlarmMelodyList> Create(
            TViewModel& viewModel,
            const TItemsSource& itemsSource,
            xaml::BindingScope& bindings) {
            auto control = std::make_unique<AlarmMelodyList>();
            control->itemsSource.Set(static_cast<const void*>(&itemsSource));
            control->InitializeComponent(
                xaml::generated::AlarmMelodyListXaml::BuildContent(viewModel, itemsSource, bindings));
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

    private:
        xaml::DependentProperty<const void*> itemsSource;
        xaml::Element* openedItem = nullptr;
    };
}