#pragma once
#include <cstdint>

struct MobileClockUiNativeBridge;
struct MobileClockUiRebuildState;

struct MobileClockUiNativeBridgeApi {
    std::uint32_t abiVersion;

    MobileClockUiNativeBridge* (*Create)();
    void (*Destroy)(MobileClockUiNativeBridge* bridge);

    void (*Attach)(
        MobileClockUiNativeBridge* bridge,
        void* root,
        const char* className);
    void (*Detach)(MobileClockUiNativeBridge* bridge, void* root);

    MobileClockUiRebuildState* (*CaptureRebuildState)(
        MobileClockUiNativeBridge* bridge,
        void* target);
    int (*RestoreRebuildState)(
        MobileClockUiNativeBridge* bridge,
        MobileClockUiRebuildState* state,
        void* pageRoot,
        void* animations);
    void (*DestroyRebuildState)(MobileClockUiRebuildState* state);
};

using MobileClockUiNativeBridgeGetApi = const MobileClockUiNativeBridgeApi* (*)(
    std::uint32_t requestedAbiVersion);

extern "C" __declspec(dllexport) const MobileClockUiNativeBridgeApi* MobileClockUiNativeBridge_GetApi(
    std::uint32_t requestedAbiVersion);