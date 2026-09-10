#include "UI/InputDispatcher.h"

#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Animation.h>
#include <XamlRuntime/Input.h>

#include "MobileClock.UI/Controls/IGestureTarget.h"

#include <cmath>
#include <memory>

namespace mobileclock::ui::_details {
    IGestureTarget* FindGestureTarget(xaml::Element* element) {
        while (element != nullptr) {
            if (auto* const target = dynamic_cast<IGestureTarget*>(element)) {
                return target;
            }
            element = element->Parent();
        }
        return nullptr;
    }

    bool UpdateGestureTargets(
        xaml::Element& element,
        xaml::Element& pageRoot,
        xaml::AnimationController& animations) {
        bool wasUpdated = false;
        if (auto* const target = dynamic_cast<IGestureTarget*>(&element)) {
            target->UpdateGestures(pageRoot, animations);
            wasUpdated = true;
        }
        for (const std::unique_ptr<xaml::Element>& child : element.Children()) {
            if (UpdateGestureTargets(*child, pageRoot, animations)) {
                wasUpdated = true;
            }
        }
        return wasUpdated;
    }
}

namespace mobileclock::ui {
    //
    // API
    //
    void InputDispatcher::PointerDown(
        xaml::Element& root,
        float x,
        float y,
        xaml::AnimationController* animations) {
        if (animations == nullptr) {
            return;
        }
        this->scrollViewer = xaml::HitTestVisual(root, x, y);
        while (this->scrollViewer != nullptr
            && this->scrollViewer->Type() != xaml::ElementType::scrollViewer) {
            this->scrollViewer = this->scrollViewer->Parent();
        }
        this->scrollController.Cancel();
        this->touchDownX = x;
        this->touchDownY = y;
        this->lastTouchY = y;
        this->gestureAxis = GestureAxis::none;
        this->interactionController.SetPanTargetPredicate([](const xaml::Element& element) {
            IGestureTarget* const target = _details::FindGestureTarget(const_cast<xaml::Element*>(&element));
            return target != nullptr && target->CanHandlePan(element);
        });
        this->interactionController.PointerDown(root, *animations, x, y);
    }

    bool InputDispatcher::PointerMove(float x, float y) {
        const float horizontalDistance = x - this->touchDownX;
        const float verticalDistance = y - this->touchDownY;
        constexpr float gestureThreshold = 8.0f;
        if (this->gestureAxis == GestureAxis::none
            && this->scrollViewer != nullptr
            && std::abs(verticalDistance) > std::abs(horizontalDistance)
            && std::abs(verticalDistance) >= gestureThreshold) {
            this->gestureAxis = GestureAxis::vertical;
            this->scrollController.Begin(*this->scrollViewer);
            this->interactionController.Cancel();
        }
        if (this->gestureAxis == GestureAxis::vertical) {
            const bool wasScrolled = this->scrollController.Drag(this->lastTouchY - y);
            this->lastTouchY = y;
            return wasScrolled;
        }
        return this->interactionController.PointerMove(x, y);
    }

    xaml::Element* InputDispatcher::PointerUp(
        xaml::Element& root,
        float x,
        float y,
        xaml::AnimationController& animations) {
        if (this->gestureAxis == GestureAxis::vertical) {
            this->scrollController.End();
            this->scrollViewer = nullptr;
            this->gestureAxis = GestureAxis::none;
            return nullptr;
        }
        this->scrollViewer = nullptr;
        const xaml::GestureResult result = this->interactionController.PointerUp(root, animations, x, y);
        if (result.kind == xaml::GestureKind::pan) {
            IGestureTarget* const target = _details::FindGestureTarget(result.target);
            if (target != nullptr && target->HandleGesture(result, animations)) {
                return nullptr;
            }
        }
        return result.kind == xaml::GestureKind::tap ? result.target : nullptr;
    }

    void InputDispatcher::Cancel() {
        this->interactionController.Cancel();
        this->scrollController.Cancel();
        this->scrollViewer = nullptr;
        this->gestureAxis = GestureAxis::none;
    }

    bool InputDispatcher::Update(xaml::Element& pageRoot, xaml::AnimationController& animations) {
        const bool interactionUpdated = this->interactionController.Update();
        const bool scrollUpdated = this->scrollController.Update();
        const bool gesturesUpdated = _details::UpdateGestureTargets(pageRoot, pageRoot, animations);
        return interactionUpdated || scrollUpdated || gesturesUpdated;
    }
}