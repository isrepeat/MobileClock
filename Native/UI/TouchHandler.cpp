#include <Helpers.Logging/Logging.h>

#include <XamlRuntime/Animation.h>
#include <XamlRuntime/Input.h>

#include "TouchHandler.h"

#include <chrono>
#include <cmath>

namespace mobileclock::ui {
    //
    // API
    //
    void TouchHandler::HandleTouchDown(
        xaml::Element& root,
        float x,
        float y,
        xaml::AnimationController* animations) {
        xaml::Element* const visualElement = xaml::HitTestVisual(root, x, y);
        this->capturedElement = xaml::HitTest(root, x, y);
        if (this->capturedElement == nullptr) {
            this->capturedElement = visualElement;
        }
        this->scrollViewer = visualElement;
        while (this->scrollViewer != nullptr && this->scrollViewer->Type() != xaml::ElementType::scrollViewer) {
            this->scrollViewer = this->scrollViewer->Parent();
        }
        this->gestureAxis = GestureAxis::none;
        this->scrollController.Cancel();
        this->touchDownX = x;
        this->touchDownY = y;
        this->lastTouchY = y;
        LOG_DEBUG(
            "MobileClock.Touch",
            "Touch down: point=({}, {}), captured='{}', scrollViewer={}",
            x,
            y,
            this->capturedElement == nullptr ? "" : this->capturedElement->Id(),
            this->scrollViewer != nullptr);
        if (this->scrollViewer != nullptr) {
            const xaml::Size extent = this->scrollViewer->Extent();
            const xaml::Size viewport = this->scrollViewer->Viewport();
            LOG_DEBUG(
                "MobileClock.Touch",
                "Scroll metrics: offset=({}, {}), extent=({}, {}), viewport=({}, {})",
                this->scrollViewer->HorizontalOffset(),
                this->scrollViewer->VerticalOffset(),
                extent.width,
                extent.height,
                viewport.width,
                viewport.height);
        }
        if (this->capturedElement != nullptr && animations != nullptr) {
            animations->Start(*this->capturedElement, xaml::AnimationTrigger::pointerDown);
        }
    }

    bool TouchHandler::HandleTouchMove(float x, float y) {
        if (this->capturedElement == nullptr) {
            LOG_DEBUG(
                "MobileClock.Touch",
                "Touch move ignored: point=({}, {}), reason=no captured element",
                x,
                y);
            return false;
        }
        const float horizontalDistance = x - this->touchDownX;
        const float verticalDistance = y - this->touchDownY;
        constexpr float gestureThreshold = 8.0f;
        if (this->gestureAxis == GestureAxis::none) {
            if (std::max(std::abs(horizontalDistance), std::abs(verticalDistance)) < gestureThreshold) {
                return false;
            }
            if (std::abs(verticalDistance) > std::abs(horizontalDistance) && this->scrollViewer != nullptr) {
                this->gestureAxis = GestureAxis::vertical;
                this->scrollController.Begin(*this->scrollViewer);
            } else if (std::abs(horizontalDistance) > std::abs(verticalDistance)
                && this->capturedElement->Id() == "alarmBlock") {
                this->gestureAxis = GestureAxis::horizontal;
            } else {
                LOG_DEBUG(
                    "MobileClock.Touch",
                    "Gesture rejected: captured='{}', horizontalDistance={}, verticalDistance={}, hasScrollViewer={}",
                    this->capturedElement->Id(),
                    horizontalDistance,
                    verticalDistance,
                    this->scrollViewer != nullptr);
                return false;
            }
            LOG_DEBUG(
                "MobileClock.Touch",
                "Gesture axis locked: element='{}', axis={}",
                this->capturedElement->Id(),
                this->gestureAxis == GestureAxis::horizontal ? "horizontal" : "vertical");
        }
        if (this->gestureAxis == GestureAxis::vertical) {
            const float verticalDelta = y - this->lastTouchY;
            const bool didScroll = this->scrollController.Drag(-verticalDelta);
            this->lastTouchY = y;
            if (!didScroll) {
                LOG_DEBUG(
                    "MobileClock.Touch",
                    "Scroll drag clamped: element='{}', verticalDelta={}, verticalOffset={}",
                    this->capturedElement->Id(),
                    -verticalDelta,
                    this->scrollViewer->VerticalOffset());
                return false;
            }
            LOG_DEBUG(
                "MobileClock.Touch",
                "Scroll drag: element='{}', verticalDelta={}, verticalOffset={}",
                this->capturedElement->Id(),
                -verticalDelta,
                this->scrollViewer->VerticalOffset());
            return true;
        }
        this->capturedElement->SetRenderOffsetX(horizontalDistance);
        LOG_DEBUG(
            "MobileClock.Touch",
            "Alarm swipe drag: element='{}', horizontalOffset={}",
            this->capturedElement->Id(),
            horizontalDistance);
        return true;
    }

    xaml::Element* TouchHandler::HandleTouchUp(
        xaml::Element& root,
        float x,
        float y,
        xaml::AnimationController& animations,
        const void*& swipedDataContext) {
        xaml::Element* const element = this->capturedElement;
        xaml::Element* const activeScrollViewer = this->scrollViewer;
        this->capturedElement = nullptr;
        this->scrollViewer = nullptr;
        swipedDataContext = nullptr;
        if (element == nullptr) {
            LOG_DEBUG("MobileClock.Touch", "Touch up ignored: reason=no captured element");
            return nullptr;
        }
        if (this->gestureAxis == GestureAxis::vertical) {
            if (element->Id() == "alarmBlock" && element->RenderOffsetX() != 0.0f) {
                LOG_DEBUG(
                    "MobileClock.Touch",
                    "Alarm swipe reset after scroll: element='{}', offset={}, target=0, duration=180ms",
                    element->Id(),
                    element->RenderOffsetX());
                animations.Animate(
                    *element,
                    xaml::AnimatedProperty::renderOffsetX,
                    element->RenderOffsetX(),
                    0.0f,
                    std::chrono::milliseconds(180));
            }
            this->scrollController.End();
            LOG_DEBUG(
                "MobileClock.Touch",
                "Scroll drag completed: element='{}', verticalOffset={}; inertial update may continue.",
                element->Id(),
                activeScrollViewer == nullptr ? 0.0f : activeScrollViewer->VerticalOffset());
            this->gestureAxis = GestureAxis::none;
            return nullptr;
        }
        // Завершаем визуальное нажатие и при отпускании за пределами кнопки.
        animations.Start(*element, xaml::AnimationTrigger::pointerUp);
        const float horizontalDistance = x - this->touchDownX;
        if (std::abs(horizontalDistance) >= 180.0f
            && this->gestureAxis == GestureAxis::horizontal) {
            swipedDataContext = element->DataContext();
            const xaml::Rect rootBounds = root.Bounds();
            const xaml::Rect elementBounds = element->Bounds();
            const float targetOffset = horizontalDistance < 0.0f
                ? -elementBounds.x - elementBounds.width
                : rootBounds.width - elementBounds.x;
            animations.Animate(
                *element,
                xaml::AnimatedProperty::renderOffsetX,
                element->RenderOffsetX(),
                targetOffset,
                std::chrono::milliseconds(220));
            return nullptr;
        }
        if (element->Id() == "alarmBlock") {
            LOG_DEBUG(
                "MobileClock.Touch",
                "Alarm swipe reset: element='{}', offset={}, target=0, duration=180ms",
                element->Id(),
                element->RenderOffsetX());
            animations.Animate(
                *element,
                xaml::AnimatedProperty::renderOffsetX,
                element->RenderOffsetX(),
                0.0f,
                std::chrono::milliseconds(180));
            return nullptr;
        }
        // Нажатие принимается только на том элементе, где началось касание.
        if (xaml::HitTest(root, x, y) != element) {
            return nullptr;
        }
        if (!xaml::HandleTap(*element)) {
            return nullptr;
        }
        if (element->Type() == xaml::ElementType::toggleSwitch) {
            animations.Start(*element, xaml::AnimationTrigger::toggled);
        }
        return element;
    }

    void TouchHandler::CancelTouch() {
        this->capturedElement = nullptr;
        this->scrollViewer = nullptr;
        this->gestureAxis = GestureAxis::none;
        this->scrollController.Cancel();
    }

    bool TouchHandler::Update() {
        return this->scrollController.Update();
    }
}