#include "UI/InputDispatcher.h"

#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <Helpers.Logging/Logging.h>
#endif
#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Animation.h>
#include <XamlRuntime/Input.h>

#include "MobileClock.UI/Controls/IGestureTarget.h"

#include <cmath>

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
        // The hit-tested element is captured for the complete pointer sequence.
        // It can move outside the cursor before the horizontal pan completes.
        xaml::Element* const target = xaml::HitTest(root, x, y);
        this->inputRoot = &root;
        this->panElement = target;
        this->panTarget = target == nullptr ? nullptr : IGestureTarget::Find(*target);
        this->scrollViewer = xaml::HitTestVisual(root, x, y);
        while (this->scrollViewer != nullptr
            && this->scrollViewer->Type() != xaml::ElementType::scrollViewer) {
            this->scrollViewer = this->scrollViewer->Parent();
        }
        if (this->scrollViewer == nullptr) {
            if (target != nullptr) {
                this->scrollViewer = IGestureTarget::FindContainingScrollViewer(*target);
            }
        }
        this->scrollController.Cancel();
        this->touchDownX = x;
        this->touchDownY = y;
        this->lastTouchX = x;
        this->lastTouchY = y;
        this->gestureAxis = GestureAxis::none;
        // IGestureTarget owns application-specific horizontal pans. Disable the
        // generic runtime pan recognizer so it cannot apply a second offset.
        this->interactionController.SetPanTargetPredicate([](const xaml::Element&) {
            return false;
        });
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        LOG_DEBUG(
            "MobileClock.Input",
            "Pointer down: target='{}', gestureTarget={}",
            target == nullptr ? "" : target->Id(),
            target != nullptr && IGestureTarget::Find(*target) != nullptr);
#endif
        this->interactionController.PointerDown(root, *animations, x, y);
    }

    bool InputDispatcher::PointerMove(float x, float y) {
        const float horizontalDistance = x - this->touchDownX;
        const float verticalDistance = y - this->touchDownY;
        constexpr float gestureThreshold = 8.0f;
        // The first movement that crosses the threshold locks the sequence to
        // scrolling. It also cancels tap recognition for this pointer sequence.
        if (this->gestureAxis == GestureAxis::none
            && this->scrollViewer != nullptr
            && std::abs(verticalDistance) > std::abs(horizontalDistance)
            && std::abs(verticalDistance) >= gestureThreshold) {
            this->gestureAxis = GestureAxis::vertical;
            this->scrollController.Begin(*this->scrollViewer);
            this->interactionController.Cancel();
        }
        // A registered target receives every horizontal pan phase and therefore
        // controls its own live visual state instead of the runtime doing so.
        if (this->gestureAxis == GestureAxis::none
            && this->panTarget != nullptr
            && this->panElement != nullptr
            && std::abs(horizontalDistance) > std::abs(verticalDistance)
            && std::abs(horizontalDistance) >= gestureThreshold) {
            this->gestureAxis = GestureAxis::horizontal;
            this->interactionController.Cancel();
            this->panTarget->BeginPan({
                *this->inputRoot,
                *this->panElement,
                this->touchDownX,
                this->touchDownY,
                this->lastTouchX,
                this->lastTouchY,
                x,
                y,
            });
        }
        if (this->gestureAxis == GestureAxis::vertical) {
            const bool wasScrolled = this->scrollController.Drag(this->lastTouchY - y);
            this->lastTouchY = y;
            return wasScrolled;
        }
        if (this->gestureAxis == GestureAxis::horizontal) {
            this->panTarget->UpdatePan({
                *this->inputRoot,
                *this->panElement,
                this->touchDownX,
                this->touchDownY,
                this->lastTouchX,
                this->lastTouchY,
                x,
                y,
            });
            this->lastTouchX = x;
            this->lastTouchY = y;
            return true;
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
            this->panTarget = nullptr;
            this->inputRoot = nullptr;
            this->panElement = nullptr;
            this->gestureAxis = GestureAxis::none;
            return nullptr;
        }
        // EndPan decides whether to commit the gesture or animate the target back.
        if (this->gestureAxis == GestureAxis::horizontal) {
            const bool wasHandled = this->panTarget->EndPan({
                *this->inputRoot,
                *this->panElement,
                this->touchDownX,
                this->touchDownY,
                this->lastTouchX,
                this->lastTouchY,
                x,
                y,
            }, animations);
#if defined(MOBILECLOCK_XAML_PREVIEWER)
            LOG_DEBUG(
                "MobileClock.Input",
                "Pan completed: target='{}', handled={}",
                this->panElement->Id(),
                wasHandled);
#endif
            this->scrollViewer = nullptr;
            this->panTarget = nullptr;
            this->inputRoot = nullptr;
            this->panElement = nullptr;
            this->gestureAxis = GestureAxis::none;
            return nullptr;
        }
        this->scrollViewer = nullptr;
        const xaml::GestureResult result = this->interactionController.PointerUp(root, animations, x, y);
        this->panTarget = nullptr;
        this->inputRoot = nullptr;
        this->panElement = nullptr;
        return result.kind == xaml::GestureKind::tap ? result.target : nullptr;
    }

    void InputDispatcher::Cancel() {
        // Android can cancel a pointer sequence without PointerUp, for example
        // when the surface loses the gesture to another system interaction.
        if (this->gestureAxis == GestureAxis::horizontal
            && this->panTarget != nullptr
            && this->panElement != nullptr) {
            this->panTarget->CancelPan(*this->panElement);
        }
        this->interactionController.Cancel();
        this->scrollController.Cancel();
        this->scrollViewer = nullptr;
        this->panTarget = nullptr;
        this->inputRoot = nullptr;
        this->panElement = nullptr;
        this->gestureAxis = GestureAxis::none;
    }

    bool InputDispatcher::Update(xaml::Element& pageRoot, xaml::AnimationController& animations) {
        const bool interactionUpdated = this->interactionController.Update();
        const bool scrollUpdated = this->scrollController.Update();
        IGestureTarget::Update(pageRoot, animations);
        return interactionUpdated || scrollUpdated;
    }
}