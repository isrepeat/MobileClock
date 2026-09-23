#pragma once
#include <XamlRuntime/XamlLayout.h>

#include "ScrollableListBase.h"

#include <string_view>
#include <functional>
#include <cstddef>
#include <chrono>
#include <vector>

namespace mobileclock::ui::base {
    class InteractiveListBase : public ScrollableListBase {
    public:
        InteractiveListBase() = default;
        ~InteractiveListBase() override = default;

    protected:
        void SetRemoveHandler(std::function<bool(const void*)> value);
        bool RequestRemoval(xaml::Element& element);

        virtual interface::GestureHandling ResolveInteractiveGesture(
            const interface::IGestureTarget::PanState& state,
            interface::GestureDirection direction) const = 0;
        virtual void BeginInteractiveGesture(const interface::IGestureTarget::PanState& state) = 0;
        virtual void UpdateInteractiveGesture(const interface::IGestureTarget::PanState& state) = 0;
        virtual bool EndInteractiveGesture(const interface::IGestureTarget::PanState& state, xaml::AnimationController& animationController) = 0;
        virtual void CancelInteractiveGesture(xaml::Element& element) = 0;
        virtual std::string_view ListViewId() const = 0;

    private:
        struct RemovalState {
            std::vector<xaml::Rect> previousBounds;
            xaml::Size scrollExtent;
            float horizontalOffset = 0.0f;
            float verticalOffset = 0.0f;
            size_t removedIndex = 0;
            bool isPresent = false;
            bool isAtBottom = false;
        };

        //
        // IGestureTarget
        //
        interface::GestureHandling ResolveGesture(const interface::IGestureTarget::PanState& state, interface::GestureDirection direction) const override;
        void BeginGesture(const interface::IGestureTarget::PanState& state) override;
        void UpdateGesture(const interface::IGestureTarget::PanState& state) override;
        bool EndGesture(const interface::IGestureTarget::PanState& state, xaml::AnimationController& animationController) override;
        void CancelGesture(xaml::Element& element) override;
        void UpdateGestures(xaml::Element& pageRoot, xaml::AnimationController& animationController) override;

        const void* FindItemDataContext(xaml::Element& element) const;
        RemovalState CaptureRemovalState(const void* dataContext) const;
        void UpdateRemoval(xaml::Element& pageRoot, xaml::AnimationController& animationController);
        void RestoreViewportAndAnimate(
            const RemovalState& state,
            xaml::Element& pageRoot,
            xaml::AnimationController& animationController,
            std::chrono::milliseconds duration);
        void RestoreViewport(const RemovalState& state, xaml::Element& pageRoot);
        void AnimateRemainingItems(
            const RemovalState& state,
            xaml::AnimationController& animationController,
            std::chrono::milliseconds duration);

    private:
        std::function<bool(const void*)> removeHandler;
        const void* pendingRemoval = nullptr;
        RemovalState pendingRemovalState;
        std::chrono::steady_clock::time_point pendingRemovalAt;
    };
}