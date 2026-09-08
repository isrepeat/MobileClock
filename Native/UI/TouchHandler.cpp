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
        this->capturedElement = xaml::HitTest(root, x, y);
        this->touchDownX = x;
        this->touchDownY = y;
        if (this->capturedElement != nullptr && animations != nullptr) {
            animations->Start(*this->capturedElement, xaml::AnimationTrigger::pointerDown);
        }
    }

    bool TouchHandler::HandleTouchMove(float x, float y) {
        if (this->capturedElement == nullptr || this->capturedElement->Id() != "alarmBlock") {
            return false;
        }
        const float horizontalDistance = x - this->touchDownX;
        const float verticalDistance = y - this->touchDownY;
        if (std::abs(horizontalDistance) <= std::abs(verticalDistance)) {
            return false;
        }
        this->capturedElement->SetRenderOffsetX(horizontalDistance);
        return true;
    }

    xaml::Element* TouchHandler::HandleTouchUp(
        xaml::Element& root,
        float x,
        float y,
        xaml::AnimationController& animations,
        const void*& swipedDataContext) {
        xaml::Element* const element = this->capturedElement;
        this->capturedElement = nullptr;
        swipedDataContext = nullptr;
        if (element == nullptr) {
            return nullptr;
        }
        // Завершаем визуальное нажатие и при отпускании за пределами кнопки.
        animations.Start(*element, xaml::AnimationTrigger::pointerUp);
        const float horizontalDistance = x - this->touchDownX;
        const float verticalDistance = y - this->touchDownY;
        if (std::abs(horizontalDistance) >= 180.0f
            && std::abs(horizontalDistance) > std::abs(verticalDistance)) {
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
    }
}