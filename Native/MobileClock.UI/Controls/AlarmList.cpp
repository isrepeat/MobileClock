#include "MobileClock.UI/Controls/AlarmList.h"

#include <XamlRuntime/Animation.h>

#include <string_view>
#include <algorithm>
#include <utility>
#include <memory>

namespace mobileclock::ui::controls {
    //
    // API
    //
    const xaml::DependentProperty<const void*>& AlarmList::ItemsSourceProperty() const {
        return this->itemsSource;
    }

    AlarmList::RemovalTransition AlarmList::PrepareRemoval(
        xaml::Element& controlRoot,
        const void* dataContext) {
        xaml::Element* const list = FindElement(controlRoot, "alarms");
        xaml::Element* const scrollViewer = FindElement(controlRoot, "alarmsScrollViewer");
        if (list == nullptr || scrollViewer == nullptr) {
            return {};
        }
        RemovalTransition transition;
        transition.scrollExtent = scrollViewer->Extent();
        transition.horizontalOffset = scrollViewer->HorizontalOffset();
        transition.verticalOffset = scrollViewer->VerticalOffset();
        const auto& items = list->Children();
        for (size_t index = 0; index < items.size(); ++index) {
            transition.previousBounds.push_back(items[index]->Bounds());
            if (items[index]->DataContext() == dataContext) {
                transition.removedIndex = index;
                transition.isPresent = true;
            }
        }
        return transition;
    }

    bool AlarmList::RemoveItem(
        xaml::Element& controlRoot,
        xaml::Element& pageRoot,
        const RemovalTransition& transition,
        xaml::AnimationController& animations,
        std::chrono::milliseconds duration) {
        if (!transition.isPresent) {
            return false;
        }
        xaml::Element* const list = FindElement(controlRoot, "alarms");
        xaml::Element* const scrollViewer = FindElement(controlRoot, "alarmsScrollViewer");
        if (list == nullptr
            || scrollViewer == nullptr
            || transition.removedIndex >= list->Children().size()
            || transition.removedIndex >= transition.previousBounds.size()) {
            return false;
        }
        scrollViewer->HoldScrollExtent(transition.scrollExtent);
        scrollViewer->SetHorizontalOffset(transition.horizontalOffset);
        scrollViewer->SetVerticalOffset(transition.verticalOffset);
        list->RemoveChild(*list->Children()[transition.removedIndex]);
        const xaml::Rect pageBounds = pageRoot.Bounds();
        if (pageBounds.width > 0.0f && pageBounds.height > 0.0f) {
            xaml::layout(pageRoot, {pageBounds.width, pageBounds.height});
        }
        animations.ReleaseScrollExtentAfter(*scrollViewer, duration);
        const auto& items = list->Children();
        const size_t count = std::min(items.size(), transition.previousBounds.size() - transition.removedIndex - 1);
        for (size_t index = 0; index < count; ++index) {
            xaml::Element& item = *items[transition.removedIndex + index];
            const float offsetY = transition.previousBounds[transition.removedIndex + index + 1].y - item.Bounds().y;
            item.SetRenderOffsetY(offsetY);
            animations.Animate(item, xaml::AnimatedProperty::renderOffsetY, offsetY, 0.0f, duration);
        }
        return true;
    }

    AlarmList::RemovalTransition AlarmList::PrepareRemoval(const void* dataContext) const {
        return PrepareRemoval(*const_cast<AlarmList*>(this), dataContext);
    }

    bool AlarmList::RemoveItem(
        const RemovalTransition& transition,
        xaml::Element& pageRoot,
        xaml::AnimationController& animations,
        std::chrono::milliseconds duration) {
        return RemoveItem(*this, pageRoot, transition, animations, duration);
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