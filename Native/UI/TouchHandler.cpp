#include <XamlRuntime/Animation.h>
#include <XamlRuntime/Input.h>

#include "TouchHandler.h"

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
        if (this->capturedElement != nullptr && animations != nullptr) {
            animations->Start(*this->capturedElement, xaml::AnimationTrigger::pointerDown);
        }
    }

    xaml::Element* TouchHandler::HandleTouchUp(
        xaml::Element& root,
        float x,
        float y,
        xaml::AnimationController& animations) {
        xaml::Element* const element = this->capturedElement;
        this->capturedElement = nullptr;
        if (element == nullptr) {
            return nullptr;
        }
        // Завершаем визуальное нажатие и при отпускании за пределами кнопки.
        animations.Start(*element, xaml::AnimationTrigger::pointerUp);
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