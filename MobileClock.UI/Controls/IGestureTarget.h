#pragma once
#include <XamlRuntime/GestureRecognizer.h>

namespace xaml {
    class AnimationController;
    class Element;
}

namespace mobileclock::ui {
    class IGestureTarget {
    public:
        virtual ~IGestureTarget() = default;

        virtual bool CanHandlePan(const xaml::Element& element) const = 0;
        virtual bool HandleGesture(const xaml::GestureResult& gesture, xaml::AnimationController& animations) = 0;
        virtual void UpdateGestures(xaml::Element& pageRoot, xaml::AnimationController& animations) = 0;
    };
}