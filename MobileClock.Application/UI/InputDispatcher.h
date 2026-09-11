#pragma once
#include <XamlRuntime/InteractionController.h>
#include <XamlRuntime/ScrollController.h>

#include <string>

namespace xaml {
    class AnimationController;
    class Element;
}

namespace mobileclock::ui {
    class IGestureTarget;

    class InputDispatcher final {
    public:
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        struct RuntimePanState {
            std::string id;
            const void* dataContext = nullptr;
            float downX = 0;
            float downY = 0;
            float currentX = 0;
            float currentY = 0;
            bool active = false;
        };
        RuntimePanState CaptureRuntimePan() const;
        void RestoreRuntimePan(xaml::Element& root, const RuntimePanState& state);
#endif
        void PointerDown(xaml::Element& root, float x, float y, xaml::AnimationController* animations = nullptr);
        bool PointerMove(float x, float y);
        xaml::Element* PointerUp(xaml::Element& root, float x, float y, xaml::AnimationController& animations);
        void Cancel();
        bool Update(xaml::Element& pageRoot, xaml::AnimationController& animations);

    private:
        enum class GestureAxis {
            none,
            vertical,
            horizontal,
            verticalPan,
        };

    private:
        xaml::InteractionController interactionController;
        IGestureTarget* panTarget = nullptr;
        xaml::Element* inputRoot = nullptr;
        xaml::Element* panElement = nullptr;
        xaml::Element* scrollViewer = nullptr;
        xaml::ScrollController scrollController;
        float touchDownX = 0.0f;
        float touchDownY = 0.0f;
        float lastTouchX = 0.0f;
        float lastTouchY = 0.0f;
        GestureAxis gestureAxis = GestureAxis::none;
    };
}