#include "MobileClock.UI/Controls/InteractiveList.h"

#include <XamlRuntime/Animation.h>
#include <XamlRuntime/XamlLayout.h>

#include <algorithm>
#include <memory>
#include <utility>

namespace mobileclock::ui::controls::_details {
    constexpr float ScrollPositionTolerance = 1.0f;

    bool FindDataContext(
        const xaml::Element& root,
        const xaml::Element& element,
        const void* inheritedDataContext,
        const void*& result) {
        const void* const dataContext = root.DataContext() != nullptr
            ? root.DataContext()
            : inheritedDataContext;
        if (&root == &element) {
            result = dataContext;
            return true;
        }
        for (const std::unique_ptr<xaml::Element>& child : root.Children()) {
            if (FindDataContext(*child, element, dataContext, result)) {
                return true;
            }
        }
        return false;
    }
}

namespace mobileclock::ui::controls {
    //
    // IGestureTarget
    //
    GestureHandling InteractiveList::ResolveGesture(const PanState& state, GestureDirection direction) const {
        return this->ResolveInteractiveGesture(state, direction);
    }

    void InteractiveList::BeginGesture(const PanState& state) {
        this->BeginInteractiveGesture(state);
    }

    void InteractiveList::UpdateGesture(const PanState& state) {
        this->UpdateInteractiveGesture(state);
    }

    bool InteractiveList::EndGesture(const PanState& state, xaml::AnimationController& animations) {
        return this->EndInteractiveGesture(state, animations);
    }

    void InteractiveList::CancelGesture(xaml::Element& element) {
        this->CancelInteractiveGesture(element);
    }

    void InteractiveList::UpdateGestures(xaml::Element& pageRoot, xaml::AnimationController& animations) {
        this->UpdateRemoval(pageRoot, animations);
    }

    //
    // Internal
    //
    void InteractiveList::SetRemoveHandler(std::function<bool(const void*)> value) {
        this->removeHandler = std::move(value);
    }

    bool InteractiveList::RequestRemoval(xaml::Element& element) {
        if (this->pendingRemoval != nullptr) {
            return false;
        }
        const void* const dataContext = this->FindItemDataContext(element);
        const RemovalState state = this->CaptureRemovalState(dataContext);
        if (!state.isPresent) {
            return false;
        }
        this->pendingRemoval = dataContext;
        this->pendingRemovalState = state;
        this->pendingRemovalAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(220);
        return this->pendingRemoval != nullptr;
    }

    const void* InteractiveList::FindItemDataContext(xaml::Element& element) const {
        const void* dataContext = nullptr;
        if (_details::FindDataContext(*this, element, nullptr, dataContext)) {
            return dataContext;
        }
        return nullptr;
    }

    InteractiveList::RemovalState InteractiveList::CaptureRemovalState(const void* dataContext) const {
        xaml::Element* const list = this->FindElement(this->ListViewId());
        xaml::Element* const scrollViewer = this->FindElement(this->ScrollViewerId());
        if (list == nullptr || scrollViewer == nullptr || dataContext == nullptr) {
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
        const float maximumVerticalOffset = std::max(0.0f, state.scrollExtent.height - scrollViewer->Viewport().height);
        const float bottomActivationRange = state.previousBounds.empty() ? 0.0f : state.previousBounds.back().height;
        state.isAtBottom = maximumVerticalOffset > 0.0f
            && maximumVerticalOffset - state.verticalOffset <= bottomActivationRange + _details::ScrollPositionTolerance;
        return state;
    }

    void InteractiveList::UpdateRemoval(xaml::Element& pageRoot, xaml::AnimationController& animations) {
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

    void InteractiveList::RestoreViewport(const RemovalState& state, xaml::Element& pageRoot) {
        xaml::Element* const scrollViewer = this->FindElement(this->ScrollViewerId());
        if (scrollViewer == nullptr) {
            return;
        }
        if (state.isAtBottom) {
            scrollViewer->ReleaseScrollExtent();
        } else {
            scrollViewer->HoldScrollExtent(state.scrollExtent);
        }
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
        xaml::Element* const list = this->FindElement(this->ListViewId());
        xaml::Element* const scrollViewer = this->FindElement(this->ScrollViewerId());
        if (list == nullptr || state.removedIndex >= state.previousBounds.size()) {
            return;
        }
        if (scrollViewer != nullptr && !state.isAtBottom) {
            animations.ReleaseScrollExtentAfter(*scrollViewer, duration);
        }
        const auto& items = list->Children();
        const size_t count = std::min(items.size(), state.previousBounds.size() - 1);
        for (size_t index = 0; index < count; ++index) {
            xaml::Element& item = *items[index];
            const size_t previousIndex = index < state.removedIndex ? index : index + 1;
            const float offsetY = state.previousBounds[previousIndex].y - item.Bounds().y
                + (scrollViewer == nullptr ? 0.0f : scrollViewer->VerticalOffset() - state.verticalOffset);
            item.SetRenderOffsetY(offsetY);
            animations.Animate(item, xaml::AnimatedProperty::renderOffsetY, offsetY, 0.0f, duration);
        }
    }
}