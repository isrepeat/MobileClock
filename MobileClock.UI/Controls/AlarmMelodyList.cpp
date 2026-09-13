#include "MobileClock.UI/Controls/AlarmMelodyList.h"

#include <XamlRuntime/Animation.h>

#include <algorithm>
#include <chrono>

namespace mobileclock::ui::controls {
    namespace _details {
        constexpr float RevealRatio = 0.35f;
        constexpr float RevealThreshold = 0.4f;

        float RevealWidth(const xaml::Element& element) {
            return element.Bounds().width * RevealRatio;
        }
    }

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    //
    // IRuntimeReloadableControl
    //
    std::string_view AlarmMelodyList::RuntimeClassName() const {
        return "mobileclock::ui::controls::AlarmMelodyList";
    }
#endif

    //
    // API
    //
    const xaml::DependentProperty<const void*>& AlarmMelodyList::ItemsSourceProperty() const {
        return this->itemsSource;
    }

    //
    // InteractiveList
    //
    GestureHandling AlarmMelodyList::ResolveInteractiveGesture(
        const PanState& state,
        GestureDirection direction) const {
        return state.target.Id() == "melodyListItem" && direction == GestureDirection::left
            ? GestureHandling::captured : GestureHandling::ignored;
    }

    void AlarmMelodyList::BeginInteractiveGesture(const PanState& state) {
        if (this->openedItem != nullptr && this->openedItem != &state.target) {
            this->openedItem->SetRenderOffsetX(0.0f);
        }
    }

    void AlarmMelodyList::UpdateInteractiveGesture(const PanState& state) {
        const float offset = std::clamp(
            state.currentX - state.downX,
            -_details::RevealWidth(state.target),
            0.0f);
        state.target.SetRenderOffsetX(offset);
    }

    bool AlarmMelodyList::EndInteractiveGesture(const PanState& state, xaml::AnimationController& animations) {
        const float revealWidth = _details::RevealWidth(state.target);
        const bool shouldReveal = state.currentX - state.downX < -revealWidth * _details::RevealThreshold;
        const float targetOffset = shouldReveal ? -revealWidth : 0.0f;
        animations.Animate(
            state.target,
            xaml::AnimatedProperty::renderOffsetX,
            state.target.RenderOffsetX(),
            targetOffset,
            std::chrono::milliseconds(180));
        this->openedItem = shouldReveal ? &state.target : nullptr;
        return true;
    }

    void AlarmMelodyList::CancelInteractiveGesture(xaml::Element& element) {
        element.SetRenderOffsetX(0.0f);
        if (this->openedItem == &element) {
            this->openedItem = nullptr;
        }
    }

    //
    // Internal
    //
    std::string_view AlarmMelodyList::ScrollViewerId() const {
        return "scrollableListScrollViewer";
    }

    std::string_view AlarmMelodyList::ListViewId() const {
        return "melodyChoices";
    }
}