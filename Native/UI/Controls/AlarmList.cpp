#include <XamlRuntime/Animation.h>
#include <XamlRuntime/XamlLayout.h>

#include "UI/Controls/AlarmList.h"

#include <algorithm>

namespace mobileclock::ui::controls::_details {
    xaml::Element* FindElement(xaml::Element& element, std::string_view id) {
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

namespace mobileclock::ui::controls {
    //
    // API
    //
    const xaml::DependentProperty<const void*>& AlarmList::ItemsSourceProperty() const {
        return this->itemsSource;
    }

    bool AlarmList::RequestRemove(
        const void* dataContext,
        const std::function<bool()>& remove,
        const std::function<AlarmList&()>& restoredList,
        xaml::AnimationController& animations,
        std::chrono::milliseconds duration) {
        const RemovalState state = this->CaptureRemovalState(dataContext);
        if (!state.isPresent || !remove()) {
            return false;
        }
        AlarmList& current = restoredList();
        current.RestoreViewport(state);
        current.AnimateRemainingItems(state, animations, duration);
        return true;
    }

    //
    // Internal
    //
    AlarmList::RemovalState AlarmList::CaptureRemovalState(const void* dataContext) const {
        xaml::Element* const list = this->FindElement("alarms");
        xaml::Element* const scrollViewer = this->FindElement("alarmsScrollViewer");
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

    void AlarmList::RestoreViewport(const RemovalState& state) {
        xaml::Element* const scrollViewer = this->FindElement("alarmsScrollViewer");
        if (scrollViewer == nullptr) {
            return;
        }
        scrollViewer->HoldScrollExtent(state.scrollExtent);
        scrollViewer->SetHorizontalOffset(state.horizontalOffset);
        scrollViewer->SetVerticalOffset(state.verticalOffset);
        const xaml::Rect bounds = this->Bounds();
        if (bounds.width > 0.0f && bounds.height > 0.0f) {
            xaml::layout(*this, {bounds.width, bounds.height});
        }
    }

    void AlarmList::AnimateRemainingItems(
        const RemovalState& state,
        xaml::AnimationController& animations,
        std::chrono::milliseconds duration) {
        xaml::Element* const list = this->FindElement("alarms");
        xaml::Element* const scrollViewer = this->FindElement("alarmsScrollViewer");
        if (list == nullptr || state.removedIndex >= state.previousBounds.size()) {
            return;
        }
        if (scrollViewer != nullptr) {
            animations.ReleaseScrollExtentAfter(*scrollViewer, duration);
        }
        const auto& items = list->Children();
        const size_t count = std::min(items.size(), state.previousBounds.size() - state.removedIndex - 1);
        for (size_t index = 0; index < count; ++index) {
            xaml::Element& item = *items[state.removedIndex + index];
            const float offsetY = state.previousBounds[state.removedIndex + index + 1].y - item.Bounds().y;
            item.SetRenderOffsetY(offsetY);
            animations.Animate(item, xaml::AnimatedProperty::renderOffsetY, offsetY, 0.0f, duration);
        }
    }

    xaml::Element* AlarmList::FindElement(std::string_view id) const {
        return _details::FindElement(*const_cast<AlarmList*>(this), id);
    }

    void AlarmList::OnInitialized() {
    }
}