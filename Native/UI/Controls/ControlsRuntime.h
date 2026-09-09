#pragma once

#include "UI/Controls/ControlRebuildParticipant.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace xaml {
    class AnimationController;
    class Element;
}

namespace mobileclock::ui::controls {
    class ControlsRuntime final {
    public:
        class RebuildState final {
        public:
            RebuildState() = default;
            ~RebuildState() = default;

        private:
            friend class ControlsRuntime;

            std::string instanceId;
            const IControlRebuildParticipant* participant = nullptr;
            std::unique_ptr<ControlRebuildState> value;
        };

        ControlsRuntime();
        ~ControlsRuntime() = default;

        void Register(std::unique_ptr<IControlRebuildParticipant> participant);
        void Attach(xaml::Element& root, std::string className);
        void Detach(xaml::Element& root);
        std::unique_ptr<RebuildState> CaptureRebuildState(xaml::Element& target) const;
        bool RestoreRebuildState(
            const RebuildState& state,
            xaml::Element& pageRoot,
            xaml::AnimationController& animations) const;

    private:
        xaml::Element* FindControlRoot(xaml::Element& target) const;
        xaml::Element* FindControlRoot(
            xaml::Element& root,
            const RebuildState& state) const;

    private:
        std::vector<std::unique_ptr<IControlRebuildParticipant>> participants;
        std::unordered_map<xaml::Element*, const IControlRebuildParticipant*> controls;
    };
}