#include "MobileClock.UI/Controls/InteractiveList.h"

#include <XamlRuntime/Animation.h>

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
    const xaml::DependentProperty<const void*>& InteractiveList::ItemsSourceProperty() const {
        return this->itemsSource;
    }

    //
    // IGestureTarget
    //
    bool InteractiveList::CanHandlePan(const xaml::Element& element) const {
        return element.Id() == "interactiveListGestureTarget";
    }

    bool InteractiveList::HandleGesture(
        const xaml::GestureResult& gesture,
        xaml::AnimationController& animations) {
        static_cast<void>(animations);
        return gesture.kind == xaml::GestureKind::pan
            && gesture.target != nullptr
            && this->BeginRemoval(gesture.target->DataContext());
    }

    void InteractiveList::UpdateGestures(xaml::Element& pageRoot, xaml::AnimationController& animations) {
        this->Update(pageRoot, animations);
    }

    bool InteractiveList::BeginRemoval(const void* dataContext) {
        if (this->pendingRemoval != nullptr) {
            return false;
        }
        const RemovalState state = this->CaptureRemovalState(dataContext);
        if (!state.isPresent) {
            return false;
        }
        this->pendingRemoval = dataContext;
        this->pendingRemovalState = state;
        this->pendingRemovalAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(220);
        return true;
    }

    void InteractiveList::Update(xaml::Element& pageRoot, xaml::AnimationController& animations) {
        if (this->pendingRemoval == nullptr || std::chrono::steady_clock::now() < this->pendingRemovalAt) {
            return;
        }
        const void* const dataContext = this->pendingRemoval;
        const RemovalState state = std::move(this->pendingRemovalState);
        this->pendingRemoval = nullptr;
        if (this->removeHandler && this->removeHandler(dataContext)) {
            this->RestoreViewportAndAnimate(state, pageRoot, animations, std::chrono::milliseconds(840));
        }
    }

    //
    // Internal
    //
    InteractiveList::RemovalState InteractiveList::CaptureRemovalState(const void* dataContext) const {
        xaml::Element* const list = this->FindElement("interactiveListItems");
        xaml::Element* const scrollViewer = this->FindElement("interactiveListScrollViewer");
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

    void InteractiveList::RestoreViewportAndAnimate(
        const RemovalState& state,
        xaml::Element& pageRoot,
        xaml::AnimationController& animations,
        std::chrono::milliseconds duration) {
        if (!state.isPresent) {
            return;
        }
        this->RestoreViewport(state, pageRoot);
        this->AnimateRemainingItems(state, animations, duration);
    }

    void InteractiveList::SetRemoveHandler(std::function<bool(const void*)> value) {
        this->removeHandler = std::move(value);
    }

    void InteractiveList::RestoreViewport(const RemovalState& state, xaml::Element& pageRoot) {
        xaml::Element* const scrollViewer = this->FindElement("interactiveListScrollViewer");
        if (scrollViewer == nullptr) {
            return;
        }
        scrollViewer->HoldScrollExtent(state.scrollExtent);
        scrollViewer->SetHorizontalOffset(state.horizontalOffset);
        scrollViewer->SetVerticalOffset(state.verticalOffset);
        const xaml::Rect bounds = pageRoot.Bounds();
        if (bounds.width > 0.0f && bounds.height > 0.0f) {
            xaml::layout(pageRoot, {bounds.width, bounds.height});
        }
    }

    void InteractiveList::AnimateRemainingItems(
        const RemovalState& state,
        xaml::AnimationController& animations,
        std::chrono::milliseconds duration) {
        xaml::Element* const list = this->FindElement("interactiveListItems");
        xaml::Element* const scrollViewer = this->FindElement("interactiveListScrollViewer");
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

    xaml::Element* InteractiveList::FindElement(std::string_view id) const {
        return _details::FindElement(*const_cast<InteractiveList*>(this), id);
    }

    void InteractiveList::OnInitialized() {
    }
}