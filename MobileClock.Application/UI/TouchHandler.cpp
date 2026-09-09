#include <XamlRuntime/Animation.h>
#include <XamlRuntime/XamlLayout.h>

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
        if (animations == nullptr) {
            return;
        }
        this->interactionController.SetPanTargetPredicate([](const xaml::Element& element) {
            return element.Id() == "alarmBlock";
        });
        this->interactionController.PointerDown(root, *animations, x, y);
    }

    bool TouchHandler::HandleTouchMove(float x, float y) {
        return this->interactionController.PointerMove(x, y);
    }

    xaml::Element* TouchHandler::HandleTouchUp(
        xaml::Element& root,
        float x,
        float y,
        xaml::AnimationController& animations,
        const void*& swipedDataContext) {
        const xaml::GestureResult result = this->interactionController.PointerUp(root, animations, x, y);
        swipedDataContext = result.kind == xaml::GestureKind::pan && result.target != nullptr
            ? result.target->DataContext()
            : nullptr;
        return result.kind == xaml::GestureKind::tap ? result.target : nullptr;
    }

    void TouchHandler::CancelTouch() {
        this->interactionController.Cancel();
    }

    bool TouchHandler::Update() {
        return this->interactionController.Update();
    }
}