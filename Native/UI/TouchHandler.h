#pragma once

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

    private:
        xaml::Element* capturedElement = nullptr;
        float touchDownX = 0.0f;
        float touchDownY = 0.0f;
    };
}