#include <XamlRuntime/Animation.h>

#include "ControlsPreviewApi.h"
#include "UI/Controls/ControlsRuntime.h"

struct mc_controls_runtime {
    mobileclock::ui::controls::ControlsRuntime value;
};

struct mc_controls_rebuild_state {
    std::unique_ptr<mobileclock::ui::controls::ControlsRuntime::RebuildState> value;
};

mc_controls_runtime* mc_controls_runtime_create() {
    return new mc_controls_runtime();
}

void mc_controls_runtime_destroy(mc_controls_runtime* runtime) {
    delete runtime;
}

void mc_controls_runtime_attach(
    mc_controls_runtime* runtime,
    void* root,
    const char* className) {
    if (runtime != nullptr && root != nullptr && className != nullptr) {
        runtime->value.Attach(*static_cast<xaml::Element*>(root), className);
    }
}

void mc_controls_runtime_detach(mc_controls_runtime* runtime, void* root) {
    if (runtime != nullptr && root != nullptr) {
        runtime->value.Detach(*static_cast<xaml::Element*>(root));
    }
}

mc_controls_rebuild_state* mc_controls_runtime_capture_rebuild_state(
    mc_controls_runtime* runtime,
    void* target) {
    if (runtime == nullptr || target == nullptr) {
        return nullptr;
    }
    auto value = runtime->value.CaptureRebuildState(*static_cast<xaml::Element*>(target));
    if (!value) {
        return nullptr;
    }
    return new mc_controls_rebuild_state{std::move(value)};
}

int mc_controls_runtime_restore_rebuild_state(
    mc_controls_runtime* runtime,
    mc_controls_rebuild_state* state,
    void* pageRoot,
    void* animations) {
    if (runtime == nullptr || state == nullptr || pageRoot == nullptr || animations == nullptr) {
        return 0;
    }
    return runtime->value.RestoreRebuildState(
        *state->value,
        *static_cast<xaml::Element*>(pageRoot),
        *static_cast<xaml::AnimationController*>(animations)) ? 1 : 0;
}

void mc_controls_rebuild_state_destroy(mc_controls_rebuild_state* state) {
    delete state;
}