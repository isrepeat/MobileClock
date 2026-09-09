#include "MobileClock.UI/Controls/AlarmList.h"

#include <XamlRuntime/Animation.h>

#include <string_view>
#include <algorithm>
#include <utility>
#include <memory>

namespace mobileclock::ui::controls::_details {
    class AlarmListRebuildState final : public ControlRebuildState {
    public:
        explicit AlarmListRebuildState(AlarmList::RemovalState value)
            : value(std::move(value)) {
        }

        AlarmList::RemovalState value;
    };

    class AlarmListRebuildParticipant final : public IControlRebuildParticipant {
    public:
        //
        // IControlRebuildParticipant
        //
        std::string_view ClassName() const override {
            return "mobileclock::ui::controls::AlarmList";
        }

        std::unique_ptr<ControlRebuildState> Capture(
            xaml::Element& controlRoot,
            xaml::Element& target) const override {
            AlarmList::RemovalState state = AlarmList::CaptureRemovalState(
                controlRoot,
                target.DataContext());
            if (!state.isPresent) {
                return nullptr;
            }
            return std::make_unique<AlarmListRebuildState>(std::move(state));
        }

        void Restore(
            xaml::Element& controlRoot,
            xaml::Element& pageRoot,
            const ControlRebuildState& state,
            xaml::AnimationController& animations) const override {
            const auto& alarmListState = static_cast<const AlarmListRebuildState&>(state);
            AlarmList::RestoreViewportAndAnimate(
                controlRoot,
                pageRoot,
                alarmListState.value,
                animations,
                std::chrono::milliseconds(840));
        }
    };
}

namespace mobileclock::ui::controls {
    //
    // API
    //
    const xaml::DependentProperty<const void*>& AlarmList::ItemsSourceProperty() const {
        return this->itemsSource;
    }

    std::unique_ptr<IControlRebuildParticipant> AlarmList::CreateRebuildParticipant() {
        return std::make_unique<_details::AlarmListRebuildParticipant>();
    }

    AlarmList::RemovalState AlarmList::CaptureRemovalState(
        xaml::Element& controlRoot,
        const void* dataContext) {
        xaml::Element* const list = FindElement(controlRoot, "alarms");
        xaml::Element* const scrollViewer = FindElement(controlRoot, "alarmsScrollViewer");
        if (list == nullptr || scrollViewer == nullptr) {
            return {};
        }
        RemovalState state;
        state.scrollExtent = scrollViewer->Extent();
        state.horizontalOffset = scrollViewer->HorizontalOffset();
        state.verticalOffset = scrollViewer->VerticalOffset();
        const auto& items = list->Children();
        for (size_t index = 0; index < items.size(); ++index) {
            state.previousBounds.push_back(items[index]->Bounds());
            if (items[index]->DataContext() == dataContext) {
                state.removedIndex = index;
                state.isPresent = true;
            }
        }
        return state;
    }

    void AlarmList::RestoreViewportAndAnimate(
        xaml::Element& controlRoot,
        xaml::Element& pageRoot,
        const RemovalState& state,
        xaml::AnimationController& animations,
        std::chrono::milliseconds duration) {
        if (!state.isPresent) {
            return;
        }
        xaml::Element* const list = FindElement(controlRoot, "alarms");
        xaml::Element* const scrollViewer = FindElement(controlRoot, "alarmsScrollViewer");
        if (list == nullptr || scrollViewer == nullptr || state.removedIndex >= state.previousBounds.size()) {
            return;
        }
        scrollViewer->HoldScrollExtent(state.scrollExtent);
        scrollViewer->SetHorizontalOffset(state.horizontalOffset);
        scrollViewer->SetVerticalOffset(state.verticalOffset);
        const xaml::Rect pageBounds = pageRoot.Bounds();
        if (pageBounds.width > 0.0f && pageBounds.height > 0.0f) {
            xaml::layout(pageRoot, {pageBounds.width, pageBounds.height});
        }
        animations.ReleaseScrollExtentAfter(*scrollViewer, duration);
        const auto& items = list->Children();
        const size_t count = std::min(items.size(), state.previousBounds.size() - state.removedIndex - 1);
        for (size_t index = 0; index < count; ++index) {
            xaml::Element& item = *items[state.removedIndex + index];
            const float offsetY = state.previousBounds[state.removedIndex + index + 1].y - item.Bounds().y;
            item.SetRenderOffsetY(offsetY);
            animations.Animate(item, xaml::AnimatedProperty::renderOffsetY, offsetY, 0.0f, duration);
        }
    }

    AlarmList::RemovalState AlarmList::CaptureRemovalState(const void* dataContext) const {
        return CaptureRemovalState(*const_cast<AlarmList*>(this), dataContext);
    }

    void AlarmList::RestoreViewportAndAnimate(
        const RemovalState& state,
        xaml::Element& pageRoot,
        xaml::AnimationController& animations,
        std::chrono::milliseconds duration) {
        RestoreViewportAndAnimate(*this, pageRoot, state, animations, duration);
    }

    //
    // Internal
    //
    void AlarmList::OnInitialized() {
    }

    xaml::Element* AlarmList::FindElement(xaml::Element& element, std::string_view id) {
        if (element.Id() == id) {
            return &element;
        }
        for (const std::unique_ptr<xaml::Element>& child : element.Children()) {
            if (xaml::Element* const found = FindElement(*child, id)) {
                return found;
            }
        }
        return nullptr;
    }
}