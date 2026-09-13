#include "MobileClock.UI/Controls/AlarmMelodyList.h"

#include <XamlRuntime/Animation.h>

#include <functional>
#include <algorithm>
#include <chrono>

namespace mobileclock::ui::controls {
    namespace _details {
        constexpr float RevealRatio = 0.35f;
        constexpr float RevealThreshold = 0.4f;

        float RevealWidth(const xaml::Element& element) {
            return element.Bounds().width * RevealRatio;
        }

        void RefreshSelection(
            xaml::Element& element,
            const void* inheritedDataContext,
            const std::function<bool(const void*)>& selectionPredicate) {
            const void* const dataContext = element.DataContext() == nullptr
                ? inheritedDataContext
                : element.DataContext();
            if (element.Id() == "melodyListItem") {
                const bool isSelected = selectionPredicate && selectionPredicate(dataContext);
                element.SetBackground(isSelected
                    ? xaml::attr::Color{42.0f / 255, 47.0f / 255, 33.0f / 255, 1}
                    : xaml::attr::Color{26.0f / 255, 29.0f / 255, 22.0f / 255, 1});
                element.SetBorderColor(isSelected
                    ? xaml::attr::Color{224.0f / 255, 182.0f / 255, 77.0f / 255, 1}
                    : xaml::attr::Color{0, 0, 0, 0});
                element.SetBorderThickness(isSelected
                    ? xaml::attr::Thickness{2, 2, 2, 2}
                    : xaml::attr::Thickness{});
            }
            for (const auto& child : element.Children()) {
                RefreshSelection(*child, dataContext, selectionPredicate);
            }
        }
	} // namespace _details

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

    void AlarmMelodyList::SetSelectionPredicate(std::function<bool(const void*)> value) {
        this->selectionPredicate = std::move(value);
        _details::RefreshSelection(*this, nullptr, this->selectionPredicate);
    }

    //
    // InteractiveList
    //
    GestureHandling AlarmMelodyList::ResolveInteractiveGesture(
        const PanState& state,
        GestureDirection direction) const {
        if (state.target.Id() != "melodyListItem") {
            return GestureHandling::ignored;
        }
        if (direction == GestureDirection::left) {
            return GestureHandling::captured;
        }
        return direction == GestureDirection::right && this->openedItem == &state.target
            ? GestureHandling::captured : GestureHandling::ignored;
    }

    void AlarmMelodyList::BeginInteractiveGesture(const PanState& state) {
        if (this->openedItem != nullptr && this->openedItem != &state.target) {
            this->openedItem->SetRenderOffsetX(0.0f);
        }
    }

    void AlarmMelodyList::UpdateInteractiveGesture(const PanState& state) {
        const float initialOffset = this->openedItem == &state.target
            ? -_details::RevealWidth(state.target)
            : 0.0f;
        const float offset = std::clamp(
            initialOffset + state.currentX - state.downX,
            -_details::RevealWidth(state.target),
            0.0f);
        state.target.SetRenderOffsetX(offset);
    }

    bool AlarmMelodyList::EndInteractiveGesture(const PanState& state, xaml::AnimationController& animations) {
        const float revealWidth = _details::RevealWidth(state.target);
        const bool shouldReveal = this->openedItem != &state.target
            && state.currentX - state.downX < -revealWidth * _details::RevealThreshold;
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

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    void AlarmMelodyList::OnTemplateReplaced() {
        _details::RefreshSelection(*this, nullptr, this->selectionPredicate);
    }
#endif
}