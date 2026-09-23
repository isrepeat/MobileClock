#pragma once
#include <XamlRuntime/InteractionController.h>
#include <XamlRuntime/ScrollController.h>

#include "MobileClock.UI/Interface/IGestureTarget.h"

#include <string>

namespace xaml {
    class AnimationController;
    class Element;
}

namespace mobileclock::application::core {
    class InputDispatcher final {
    public:
#if defined(ANDROID_APP_PREVIEWER)
        struct preview_RuntimePanState {
            std::string id;
            const void* dataContext = nullptr;
            float downX = 0;
            float downY = 0;
            float currentX = 0;
            float currentY = 0;
            mobileclock::ui::interface::GestureDirection direction = mobileclock::ui::interface::GestureDirection::none;
            bool active = false;
        };
        preview_RuntimePanState preview_CaptureRuntimePan() const;
        void preview_RestoreRuntimePan(xaml::Element& root, const preview_RuntimePanState& state);
#endif
        void PointerDown(xaml::Element& root, float x, float y, xaml::AnimationController* animations = nullptr);
        bool PointerMove(float x, float y);
        xaml::Element* PointerUp(xaml::Element& root, float x, float y, xaml::AnimationController& animations);
        void Cancel();
        bool Update(xaml::Element& pageRoot, xaml::AnimationController& animations);

    private:
        enum class ActiveGesture {
            none,
            scroll,
            target,
        };

    private:
        xaml::InteractionController interactionController;
        mobileclock::ui::interface::IGestureTarget* panTarget = nullptr;
        xaml::Element* inputRoot = nullptr;
        xaml::Element* panElement = nullptr;
        xaml::Element* scrollViewer = nullptr;
        xaml::ScrollController scrollController;
        float touchDownX = 0.0f;
        float touchDownY = 0.0f;
        float lastTouchX = 0.0f;
        float lastTouchY = 0.0f;
        ActiveGesture activeGesture = ActiveGesture::none;
        mobileclock::ui::interface::GestureDirection gestureDirection = mobileclock::ui::interface::GestureDirection::none;
    };
}