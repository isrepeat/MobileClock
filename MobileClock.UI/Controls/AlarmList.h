#pragma once
#include "!Generated/MobileClock.UI/Xaml/Controls/AlarmList.xaml.h"

#include <XamlRuntime/DependentProperty.h>
#include <XamlRuntime/UserControl.h>
#include <XamlRuntime/XamlLayout.h>

#include <chrono>
#include <memory>
#include <vector>

namespace mobileclock::ui::controls {
    class AlarmList final : public xaml::UserControl {
    public:
        struct RemovalState {
            std::vector<xaml::Rect> previousBounds;
            xaml::Size scrollExtent;
            float horizontalOffset = 0.0f;
            float verticalOffset = 0.0f;
            size_t removedIndex = 0;
            bool isPresent = false;
        };

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

        RemovalState CaptureRemovalState(const void* dataContext) const;
        void RestoreViewportAndAnimate(
            const RemovalState& state,
            xaml::Element& pageRoot,
            xaml::AnimationController& animations,
            std::chrono::milliseconds duration);

    private:
        template <typename TItemsSource>
        void SetItemsSource(const TItemsSource& value) {
            this->itemsSource.Set(static_cast<const void*>(&value));
        }

        void RestoreViewport(const RemovalState& state, xaml::Element& pageRoot);
        void AnimateRemainingItems(
            const RemovalState& state,
            xaml::AnimationController& animations,
            std::chrono::milliseconds duration);
        xaml::Element* FindElement(std::string_view id) const;
        void OnInitialized() override;

    private:
        xaml::DependentProperty<const void*> itemsSource;
    };
}