#include "MobileClock.UI/Controls/InteractiveList.h"

#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeTreeBuilder.h>
#include <Helpers.Logging/Logging.h>
#endif
#include <XamlRuntime/Animation.h>

#include <algorithm>
#include <cmath>

namespace mobileclock::ui::controls::_details {
    constexpr float PanCompletionThreshold = 180.0f;
    constexpr float ScrollPositionTolerance = 1.0f;

    bool Contains(const xaml::Element& root, const xaml::Element& element) {
        if (&root == &element) {
            return true;
        }
        for (const std::unique_ptr<xaml::Element>& child : root.Children()) {
            if (Contains(*child, element)) {
                return true;
            }
        }
        return false;
    }

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
#if defined(MOBILECLOCK_XAML_PREVIEWER)
    //
    // IRuntimeReloadableControl
    //
    std::string_view InteractiveList::RuntimeClassName() const {
        return "mobileclock::ui::controls::InteractiveList";
    }

    bool InteractiveList::ReplaceTemplate(const xaml::runtime::XamlElementNode& templateNode,
        const xaml::runtime::RuntimeBindingContext& context, std::string& diagnostics) {
        try {
            const auto& content = templateNode.name == "UserControl" ? templateNode.children.at(0) : templateNode;
            auto result = xaml::runtime::RuntimeTreeBuilder{}.BuildPage(content, context,
                {this->Bounds().width, this->Bounds().height});
            xaml::Element* const newList = _details::FindElement(*result.root, "interactiveListItems");
            xaml::Element* const newScroll = _details::FindElement(*result.root, "interactiveListScrollViewer");
              if (newList == nullptr || newScroll == nullptr || newList->Type() != xaml::ElementType::listView
                  || newScroll->Type() != xaml::ElementType::scrollViewer) {
                throw std::invalid_argument("InteractiveList requires interactiveListItems and interactiveListScrollViewer");
              }
              // Прокрутка хранится у старого ScrollViewer, который будет уничтожен
              // вместе с шаблоном, поэтому переносим её до ReplaceContent.
              if (const auto* scroll = this->FindElement("interactiveListScrollViewer")) {
                newScroll->SetHorizontalOffset(scroll->HorizontalOffset());
                newScroll->SetVerticalOffset(scroll->VerticalOffset());
            }
            if (context.prepareTree) {
                context.prepareTree(*result.root);
            }
              if (context.beforeCommit) {
                  context.beforeCommit();
              }
              // Содержимое и подписки меняются только после успешного построения,
              // валидации обязательных элементов и подготовки анимаций.
              this->ReplaceContent(std::move(result.root));
            this->runtimeBindings = std::move(result.bindings);
            diagnostics.clear();
            return true;
        } catch (const std::exception& error) {
            diagnostics = error.what();
            return false;
        }
    }

    //
    // API
    //
    void InteractiveList::PreserveInstances(xaml::Element& previous, xaml::Element& replacement, xaml::BindingScope& bindings) {
        std::vector<xaml::UserControl*> oldControls;
        std::vector<xaml::UserControl*> newControls;
        const auto collect = [](auto&& self, xaml::Element& node, std::vector<xaml::UserControl*>& controls) -> void {
            if (auto* control = dynamic_cast<xaml::UserControl*>(&node)) {
                controls.push_back(control);
                return;
            }
            for (const auto& child : node.Children()) {
                self(self, *child, controls);
            }
        };
        collect(collect, previous, oldControls);
        collect(collect, replacement, newControls);
        for (auto* next : newControls) {
            const auto found = std::find_if(oldControls.begin(), oldControls.end(), [next](const auto* old) {
                return old != nullptr && old->Id() == next->Id() && typeid(*old) == typeid(*next);
            });
            if (found != oldControls.end()) {
                (*found)->CopyLayoutFrom(*next);
                bindings.RetargetRuntimeElement(*next, **found);
                (*found)->SwapTreePosition(*next);
                *found = nullptr;
            }
        }
    }
#endif
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
        // A list owns scroll bars, switches and other children too; only this
        // surface is allowed to begin the swipe-to-dismiss gesture.
        return element.Id() == "interactiveListGestureTarget";
    }

    xaml::Element* InteractiveList::FindScrollViewer(const xaml::Element& element) const {
        return this->Owns(element) ? this->FindElement("interactiveListScrollViewer") : nullptr;
    }

    void InteractiveList::BeginPan(const PanState& state) {
        // This list does not need additional state before its first live offset.
        static_cast<void>(state);
    }

    void InteractiveList::UpdatePan(const PanState& state) {
        // The target follows the pointer until EndPan commits or returns it.
        state.target.SetRenderOffsetX(state.currentX - state.downX);
    }

    bool InteractiveList::EndPan(const PanState& state, xaml::AnimationController& animations) {
        const float horizontalDistance = state.currentX - state.downX;
        if (std::abs(horizontalDistance) < _details::PanCompletionThreshold) {
            // A short swipe is cancelled visually and leaves the ViewModel intact.
            animations.Animate(
                state.target,
                xaml::AnimatedProperty::renderOffsetX,
                state.target.RenderOffsetX(),
                0.0f,
                std::chrono::milliseconds(180));
            return false;
        }
        const void* const dataContext = this->FindItemDataContext(state.target);
        const bool wasHandled = this->BeginRemoval(dataContext);
        if (!wasHandled) {
            // Do not leave the item displaced when its model cannot be removed.
            animations.Animate(
                state.target,
                xaml::AnimatedProperty::renderOffsetX,
                state.target.RenderOffsetX(),
                0.0f,
                std::chrono::milliseconds(180));
            return false;
        }
        const xaml::Rect rootBounds = state.root.Bounds();
        const xaml::Rect targetBounds = state.target.Bounds();
        const float targetOffset = horizontalDistance < 0.0f
            ? -targetBounds.x - targetBounds.width
            : rootBounds.width - targetBounds.x;
        // Keep the 220 ms exit animation aligned with the delayed model removal.
        animations.Animate(
            state.target,
            xaml::AnimatedProperty::renderOffsetX,
            state.target.RenderOffsetX(),
            targetOffset,
            std::chrono::milliseconds(220));
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        LOG_DEBUG(
            "MobileClock.InteractiveList",
            "Pan target='{}', distance={}, itemDataContext={}, removalStarted={}",
            state.target.Id(),
            horizontalDistance,
            dataContext != nullptr,
            wasHandled);
#endif
        return wasHandled;
    }

    void InteractiveList::CancelPan(xaml::Element& element) {
        // Cancellation has no animation controller; restore a safe visual state.
        element.SetRenderOffsetX(0.0f);
    }

    void InteractiveList::UpdateGestures(xaml::Element& pageRoot, xaml::AnimationController& animations) {
        this->Update(pageRoot, animations);
    }

    const void* InteractiveList::FindItemDataContext(xaml::Element& element) const {
        const void* dataContext = nullptr;
        if (_details::FindDataContext(*this, element, nullptr, dataContext)) {
            return dataContext;
        }
        return nullptr;
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
        const float maximumVerticalOffset = std::max(0.0f,
            state.scrollExtent.height - scrollViewer->Viewport().height);
        const float bottomActivationRange = state.previousBounds.empty()
            ? 0.0f
            : state.previousBounds.back().height;
        state.isAtBottom = maximumVerticalOffset > 0.0f
            && maximumVerticalOffset - state.verticalOffset
                <= bottomActivationRange + _details::ScrollPositionTolerance;
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
        xaml::Element* const list = this->FindElement("interactiveListItems");
        xaml::Element* const scrollViewer = this->FindElement("interactiveListScrollViewer");
        if (list == nullptr || state.removedIndex >= state.previousBounds.size()) {
            return;
        }
        if (scrollViewer != nullptr) {
            if (!state.isAtBottom) {
                animations.ReleaseScrollExtentAfter(*scrollViewer, duration);
            }
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

    xaml::Element* InteractiveList::FindElement(std::string_view id) const {
        return _details::FindElement(*const_cast<InteractiveList*>(this), id);
    }

    void InteractiveList::OnInitialized() {
        this->RegisterGestureTarget();
    }

    bool InteractiveList::IsIn(const xaml::Element& pageRoot) const {
        if (&pageRoot == this) {
            return true;
        }
        for (const std::unique_ptr<xaml::Element>& child : pageRoot.Children()) {
            if (this->IsIn(*child)) {
                return true;
            }
        }
        return false;
    }

    bool InteractiveList::Owns(const xaml::Element& element) const {
        return _details::Contains(*this, element);
    }
}