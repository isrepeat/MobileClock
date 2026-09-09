#pragma once
#include <XamlRuntime/InteractionController.h>
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
            vertical,
        };

    private:
        xaml::InteractionController interactionController;
        xaml::Element* scrollViewer = nullptr;
        xaml::ScrollController scrollController;
        float touchDownX = 0.0f;
        float touchDownY = 0.0f;
        float lastTouchY = 0.0f;
        GestureAxis gestureAxis = GestureAxis::none;
    };
}