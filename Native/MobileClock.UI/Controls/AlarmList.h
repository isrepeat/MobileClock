#pragma once
#include "!Generated/Xaml/Controls/AlarmList.xaml.h"

#include <XamlRuntime/DependentProperty.h>
#include <XamlRuntime/UserControl.h>
#include <XamlRuntime/XamlLayout.h>

#include "MobileClock.UI/Controls/ControlRebuildParticipant.h"

#include <string_view>
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

        static std::unique_ptr<IControlRebuildParticipant> CreateRebuildParticipant();
        static RemovalState CaptureRemovalState(xaml::Element& controlRoot, const void* dataContext);
        static void RestoreViewportAndAnimate(
            xaml::Element& controlRoot,
            xaml::Element& pageRoot,
            const RemovalState& state,
            xaml::AnimationController& animations,
            std::chrono::milliseconds duration);

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

        void OnInitialized() override;
        static xaml::Element* FindElement(xaml::Element& element, std::string_view id);

    private:
        xaml::DependentProperty<const void*> itemsSource;
    };
}