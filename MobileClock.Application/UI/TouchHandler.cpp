#include <XamlRuntime/Animation.h>
#include <XamlRuntime/Input.h>
#include <XamlRuntime/XamlLayout.h>

#include "TouchHandler.h"

#include <cmath>
#include <string_view>

namespace mobileclock::ui::_details {
    xaml::Element* FindElementById(xaml::Element& element, std::string_view id) {
        if (element.Id() == id) {
            return &element;
        }
        for (const auto& child : element.Children()) {
            if (xaml::Element* const found = FindElementById(*child, id)) {
                return found;
            }
        }
        return nullptr;
    }

    bool Contains(const xaml::Rect& bounds, float x, float y) {
        return x >= bounds.x
            && x <= bounds.x + bounds.width
            && y >= bounds.y
            && y <= bounds.y + bounds.height;
    }
}

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
        xaml::Element* const alarmScrollViewer = _details::FindElementById(root, "alarmsScrollViewer");
        if (alarmScrollViewer != nullptr && _details::Contains(alarmScrollViewer->Bounds(), x, y)) {
            this->scrollViewer = alarmScrollViewer;
        } else {
            this->scrollViewer = xaml::HitTestVisual(root, x, y);
            while (this->scrollViewer != nullptr
                && this->scrollViewer->Type() != xaml::ElementType::scrollViewer) {
                this->scrollViewer = this->scrollViewer->Parent();
            }
        }
        this->scrollController.Cancel();
        this->touchDownX = x;
        this->touchDownY = y;
        this->lastTouchY = y;
        this->gestureAxis = GestureAxis::none;
        this->interactionController.SetPanTargetPredicate([](const xaml::Element& element) {
            return element.Id() == "alarmBlock";
        });
        this->interactionController.PointerDown(root, *animations, x, y);
    }

    bool TouchHandler::HandleTouchMove(float x, float y) {
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

    xaml::Element* TouchHandler::HandleTouchUp(
        xaml::Element& root,
        float x,
        float y,
        xaml::AnimationController& animations,
        const void*& swipedDataContext) {
        if (this->gestureAxis == GestureAxis::vertical) {
            this->scrollController.End();
            this->scrollViewer = nullptr;
            this->gestureAxis = GestureAxis::none;
            swipedDataContext = nullptr;
            return nullptr;
        }
        this->scrollViewer = nullptr;
        const xaml::GestureResult result = this->interactionController.PointerUp(root, animations, x, y);
        swipedDataContext = result.kind == xaml::GestureKind::pan && result.target != nullptr
            ? result.target->DataContext()
            : nullptr;
        return result.kind == xaml::GestureKind::tap ? result.target : nullptr;
    }

    void TouchHandler::CancelTouch() {
        this->interactionController.Cancel();
        this->scrollController.Cancel();
        this->scrollViewer = nullptr;
        this->gestureAxis = GestureAxis::none;
    }

    bool TouchHandler::Update() {
        const bool interactionUpdated = this->interactionController.Update();
        const bool scrollUpdated = this->scrollController.Update();
        return interactionUpdated || scrollUpdated;
    }
}