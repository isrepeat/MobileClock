#pragma once

#include <XamlRuntime/ScrollController.h>

namespace xaml {
    class AnimationController;
    class Element;
}

namespace mobileclock::ui {
    class TouchHandler final {
    public:
        void HandleTouchDown(xaml::Element& root, float x, float y, xaml::AnimationController* animations = nullptr);
        bool HandleTouchMove(float x, float y);
        xaml::Element* HandleTouchUp(
            xaml::Element& root,
            float x,
            float y,
            xaml::AnimationController& animations,
            const void*& swipedDataContext);
        void CancelTouch();
        bool Update();

    private:
        enum class GestureAxis {
            none,
            horizontal,
            vertical,
        };

    private:
        xaml::Element* capturedElement = nullptr;
        float touchDownX = 0.0f;
        float touchDownY = 0.0f;
        float lastTouchY = 0.0f;
        xaml::Element* scrollViewer = nullptr;
        xaml::ScrollController scrollController;
        GestureAxis gestureAxis = GestureAxis::none;
    };
}