#pragma once
#include <XamlRuntime/XamlLayout.h>

#include "MobileClock.UI/Controls/ScrollableList.h"

#include <chrono>
#include <cstddef>
#include <functional>
#include <string_view>
#include <vector>

namespace mobileclock::ui::controls {
    class InteractiveList : public ScrollableList {
    public:
        InteractiveList() = default;
        ~InteractiveList() override = default;

    protected:
        void SetRemoveHandler(std::function<bool(const void*)> value);
        bool RequestRemoval(xaml::Element& element);

        virtual GestureHandling ResolveInteractiveGesture(
            const PanState& state,
            GestureDirection direction) const = 0;
        virtual void BeginInteractiveGesture(const PanState& state) = 0;
        virtual void UpdateInteractiveGesture(const PanState& state) = 0;
        virtual bool EndInteractiveGesture(const PanState& state, xaml::AnimationController& animations) = 0;
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
        GestureHandling ResolveGesture(const PanState& state, GestureDirection direction) const override;
        void BeginGesture(const PanState& state) override;
        void UpdateGesture(const PanState& state) override;
        bool EndGesture(const PanState& state, xaml::AnimationController& animations) override;
        void CancelGesture(xaml::Element& element) override;
        void UpdateGestures(xaml::Element& pageRoot, xaml::AnimationController& animations) override;

        const void* FindItemDataContext(xaml::Element& element) const;
        RemovalState CaptureRemovalState(const void* dataContext) const;
        void UpdateRemoval(xaml::Element& pageRoot, xaml::AnimationController& animations);
        void RestoreViewportAndAnimate(
            const RemovalState& state,
            xaml::Element& pageRoot,
            xaml::AnimationController& animations,
            std::chrono::milliseconds duration);
        void RestoreViewport(const RemovalState& state, xaml::Element& pageRoot);
        void AnimateRemainingItems(
            const RemovalState& state,
            xaml::AnimationController& animations,
            std::chrono::milliseconds duration);

    private:
        std::function<bool(const void*)> removeHandler;
        const void* pendingRemoval = nullptr;
        RemovalState pendingRemovalState;
        std::chrono::steady_clock::time_point pendingRemovalAt;
    };
}