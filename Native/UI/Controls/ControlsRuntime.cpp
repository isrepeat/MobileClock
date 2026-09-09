#include <XamlRuntime/Animation.h>
#include <XamlRuntime/XamlLayout.h>

#include "UI/Controls/ControlsRuntime.h"
#include "UI/Controls/MobileClockControls.h"

namespace mobileclock::ui::controls {
    //
    // API
    //
    ControlsRuntime::ControlsRuntime() {
        RegisterMobileClockControls(*this);
    }

    void ControlsRuntime::Register(std::unique_ptr<IControlRebuildParticipant> participant) {
        this->participants.push_back(std::move(participant));
    }

    void ControlsRuntime::Attach(xaml::Element& root, std::string className) {
        for (const std::unique_ptr<IControlRebuildParticipant>& participant : this->participants) {
            if (participant->ClassName() == className) {
                this->controls[&root] = participant.get();
                return;
            }
        }
    }

    void ControlsRuntime::Detach(xaml::Element& root) {
        this->controls.erase(&root);
        for (const std::unique_ptr<xaml::Element>& child : root.Children()) {
            this->Detach(*child);
        }
    }

    std::unique_ptr<ControlsRuntime::RebuildState> ControlsRuntime::CaptureRebuildState(
        xaml::Element& target) const {
        xaml::Element* const root = this->FindControlRoot(target);
        if (root == nullptr) {
            return nullptr;
        }
        const auto iterator = this->controls.find(root);
        if (iterator == this->controls.end()) {
            return nullptr;
        }
        auto state = std::make_unique<RebuildState>();
        state->instanceId = root->Id();
        state->participant = iterator->second;
        state->value = state->participant->Capture(*root, target);
        return state->value ? std::move(state) : nullptr;
    }

    bool ControlsRuntime::RestoreRebuildState(
        const RebuildState& state,
        xaml::Element& pageRoot,
        xaml::AnimationController& animations) const {
        xaml::Element* const root = this->FindControlRoot(pageRoot, state);
        if (root == nullptr) {
            return false;
        }
        state.participant->Restore(*root, pageRoot, *state.value, animations);
        return true;
    }

    //
    // Internal
    //
    xaml::Element* ControlsRuntime::FindControlRoot(xaml::Element& target) const {
        for (xaml::Element* element = &target; element != nullptr; element = element->Parent()) {
            if (this->controls.find(element) != this->controls.end()) {
                return element;
            }
        }
        return nullptr;
    }

    xaml::Element* ControlsRuntime::FindControlRoot(
        xaml::Element& root,
        const RebuildState& state) const {
        const auto control = this->controls.find(&root);
        if (control != this->controls.end()
            && control->second == state.participant
            && (state.instanceId.empty() || root.Id() == state.instanceId)) {
            return &root;
        }
        for (const std::unique_ptr<xaml::Element>& child : root.Children()) {
            if (xaml::Element* const found = this->FindControlRoot(*child, state)) {
                return found;
            }
        }
        return nullptr;
    }
}