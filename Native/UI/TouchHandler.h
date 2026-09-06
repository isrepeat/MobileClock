#pragma once

namespace xaml {
    class AnimationController;
    class Element;
}

namespace mobileclock::ui {
    class TouchHandler final {
    public:
        void HandleTouchDown(xaml::Element& root, float x, float y, xaml::AnimationController* animations = nullptr);
        xaml::Element* HandleTouchUp(xaml::Element& root, float x, float y, xaml::AnimationController& animations);
        void CancelTouch();

    private:
        xaml::Element* capturedElement = nullptr;
    };
}