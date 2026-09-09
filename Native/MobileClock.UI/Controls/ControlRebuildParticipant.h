#pragma once
#include <string_view>
#include <memory>

namespace xaml {
    class AnimationController;
    class Element;
}

namespace mobileclock::ui::controls {
    class ControlRebuildState {
    public:
        ControlRebuildState() = default;
        virtual ~ControlRebuildState() = default;
    };

    class IControlRebuildParticipant {
    public:
        IControlRebuildParticipant() = default;
        virtual ~IControlRebuildParticipant() = default;

        virtual std::string_view ClassName() const = 0;
        virtual std::unique_ptr<ControlRebuildState> Capture(
            xaml::Element& controlRoot,
            xaml::Element& target) const = 0;
        virtual void Restore(
            xaml::Element& controlRoot,
            xaml::Element& pageRoot,
            const ControlRebuildState& state,
            xaml::AnimationController& animations) const = 0;
    };
}