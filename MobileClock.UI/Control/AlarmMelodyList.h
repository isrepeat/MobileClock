#pragma once
#include <XamlRuntime/DependentProperty.h>

#include "!Generated/MobileClock.UI/Xaml/Control/AlarmMelodyList.xaml.h"
#include "../Base/InteractiveListBase.h"

#include <type_traits>
#include <string_view>
#include <functional>
#include <memory>

namespace mobileclock::ui::control {
    class AlarmMelodyList final : public base::InteractiveListBase {
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
            using Item = std::remove_cvref_t<decltype(*itemsSource.begin())>;
            control->SetSelectionPredicate([&viewModel](const void* dataContext) {
                const auto* item = static_cast<const Item*>(dataContext);
                return item != nullptr && item->Id() == viewModel.Settings().melodyId;
            });
            return control;
        }

#if defined(MOBILECLOCK_XAML_PREVIEWER)
        //
        // IRuntimeReloadableControl
        //
        std::string_view RuntimeClassName() const override;
#endif

        const xaml::DependentProperty<const void*>& ItemsSourceProperty() const;
        void SetSelectionPredicate(std::function<bool(const void*)> value);

    private:
        std::string_view ScrollViewerId() const override;
        std::string_view ListViewId() const override;
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        void OnTemplateReplaced() override;
#endif

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

    private:
        xaml::DependentProperty<const void*> itemsSource;
        std::function<bool(const void*)> selectionPredicate;
        xaml::Element* openedItem = nullptr;
    };
}