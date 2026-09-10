#pragma once
#include <XamlRuntime/DependentProperty.h>
#include <XamlRuntime/UserControl.h>
#include <XamlRuntime/XamlLayout.h>

#include "!Generated/MobileClock.UI/Xaml/Controls/InteractiveList.xaml.h"
#include "MobileClock.UI/Controls/IGestureTarget.h"

#include <chrono>
#include <functional>
#include <memory>
#include <vector>

namespace mobileclock::ui::controls {
    class InteractiveList final : public xaml::UserControl, public IGestureTarget {
    public:
        struct RemovalState {
            std::vector<xaml::Rect> previousBounds;
            xaml::Size scrollExtent;
            float horizontalOffset = 0.0f;
            float verticalOffset = 0.0f;
            size_t removedIndex = 0;
            bool isPresent = false;
        };

        InteractiveList() = default;
        ~InteractiveList() override = default;

        template <typename TViewModel, typename TItemsSource>
        static std::unique_ptr<InteractiveList> Create(
            TViewModel& viewModel,
            const TItemsSource& itemsSource,
            xaml::BindingScope& bindings) {
            auto control = std::make_unique<InteractiveList>();
            control->SetItemsSource(itemsSource);
            control->removeHandler = [&viewModel](const void* dataContext) { return viewModel.RemoveItem(dataContext); };
            control->InitializeComponent(
                xaml::generated::InteractiveListXaml::BuildContent(viewModel, itemsSource, bindings));
            return control;
        }

        const xaml::DependentProperty<const void*>& ItemsSourceProperty() const;

        bool CanHandlePan(const xaml::Element& element) const override;
        bool HandleGesture(const xaml::GestureResult& gesture, xaml::AnimationController& animations) override;
        void UpdateGestures(xaml::Element& pageRoot, xaml::AnimationController& animations) override;

    private:
        void SetRemoveHandler(std::function<bool(const void*)> value);
        bool BeginRemoval(const void* dataContext);
        RemovalState CaptureRemovalState(const void* dataContext) const;
        void Update(xaml::Element& pageRoot, xaml::AnimationController& animations);
        void RestoreViewportAndAnimate(const RemovalState& state, xaml::Element& pageRoot, xaml::AnimationController& animations, std::chrono::milliseconds duration);
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
        std::function<bool(const void*)> removeHandler;
        const void* pendingRemoval = nullptr;
        RemovalState pendingRemovalState;
        std::chrono::steady_clock::time_point pendingRemovalAt;
    };
}