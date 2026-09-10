#pragma once
#include <XamlRuntime/InteractionController.h>
#include <XamlRuntime/ScrollController.h>

namespace xaml {
    class AnimationController;
    class Element;
}

namespace mobileclock::ui {
    class InputDispatcher final {
    public:
        void PointerDown(xaml::Element& root, float x, float y, xaml::AnimationController* animations = nullptr);
        bool PointerMove(float x, float y);
        xaml::Element* PointerUp(xaml::Element& root, float x, float y, xaml::AnimationController& animations);
        void Cancel();
        bool Update(xaml::Element& pageRoot, xaml::AnimationController& animations);

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