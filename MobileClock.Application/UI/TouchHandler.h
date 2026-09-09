#pragma once
#include <XamlRuntime/InteractionController.h>

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
        xaml::InteractionController interactionController;
    };
}