#pragma once

extern "C" {
    typedef struct mc_controls_runtime mc_controls_runtime;
    typedef struct mc_controls_rebuild_state mc_controls_rebuild_state;

    __declspec(dllexport) mc_controls_runtime* mc_controls_runtime_create();
    __declspec(dllexport) void mc_controls_runtime_destroy(mc_controls_runtime* runtime);
    __declspec(dllexport) void mc_controls_runtime_attach(
        mc_controls_runtime* runtime,
        void* root,
        const char* className);
    __declspec(dllexport) void mc_controls_runtime_detach(
        mc_controls_runtime* runtime,
        void* root);
    __declspec(dllexport) mc_controls_rebuild_state* mc_controls_runtime_capture_rebuild_state(
        mc_controls_runtime* runtime,
        void* target);
    __declspec(dllexport) int mc_controls_runtime_restore_rebuild_state(
        mc_controls_runtime* runtime,
        mc_controls_rebuild_state* state,
        void* pageRoot,
        void* animations);
    __declspec(dllexport) void mc_controls_rebuild_state_destroy(mc_controls_rebuild_state* state);
}