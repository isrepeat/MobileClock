#pragma once
namespace xaml {
    class AnimationController;
    class Element;
}

namespace mobileclock::ui::interface {
    enum class GestureDirection {
        none,
        up,
        down,
        left,
        right,
        upLeft,
        upRight,
        downLeft,
        downRight,
    };

    enum class GestureHandling {
        ignored,
        scroll,
        captured,
    };

    class IGestureTarget {
    public:
        struct PanState {
            xaml::Element& root;
            xaml::Element& target;
            float downX;
            float downY;
            float previousX;
            float previousY;
            float currentX;
            float currentY;
        };

        virtual ~IGestureTarget();

        static IGestureTarget* Find(
            const xaml::Element& element,
            const PanState& state,
            GestureDirection direction);
        static xaml::Element* FindContainingScrollViewer(const xaml::Element& element);
        static void Update(xaml::Element& pageRoot, xaml::AnimationController& animations);

        virtual xaml::Element* FindScrollViewer(const xaml::Element& element) const = 0;
        virtual GestureHandling ResolveGesture(const PanState& state, GestureDirection direction) const;
        virtual void BeginGesture(const PanState& state);
        virtual void UpdateGesture(const PanState& state);
        virtual bool EndGesture(const PanState& state, xaml::AnimationController& animations);
        virtual void CancelGesture(xaml::Element& element);
        virtual void UpdateGestures(xaml::Element& pageRoot, xaml::AnimationController& animations) = 0;

    protected:
        void RegisterGestureTarget();

    private:
        virtual bool IsIn(const xaml::Element& pageRoot) const = 0;
        virtual bool Owns(const xaml::Element& element) const = 0;
    };
}