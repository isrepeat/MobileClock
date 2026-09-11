#pragma once
namespace xaml {
    class AnimationController;
    class Element;
}

namespace mobileclock::ui {
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

        static IGestureTarget* Find(const xaml::Element& element);
        static xaml::Element* FindContainingScrollViewer(const xaml::Element& element);
        static void Update(xaml::Element& pageRoot, xaml::AnimationController& animations);

        virtual bool CanHandlePan(const xaml::Element& element) const = 0;
        virtual bool IsVerticalPan() const;
        virtual xaml::Element* FindScrollViewer(const xaml::Element& element) const = 0;
        virtual void BeginPan(const PanState& state) = 0;
        virtual void UpdatePan(const PanState& state) = 0;
        virtual bool EndPan(const PanState& state, xaml::AnimationController& animations) = 0;
        virtual void CancelPan(xaml::Element& element) = 0;
        virtual void UpdateGestures(xaml::Element& pageRoot, xaml::AnimationController& animations) = 0;

    protected:
        void RegisterGestureTarget();

    private:
        virtual bool IsIn(const xaml::Element& pageRoot) const = 0;
        virtual bool Owns(const xaml::Element& element) const = 0;
    };
}