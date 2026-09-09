#pragma once
#include <cstdint>

struct MobileClockUiNativeBridge;

struct MobileClockUiNativeBridgeApi {
    std::uint32_t abiVersion;

    MobileClockUiNativeBridge* (*Create)();
    void (*Destroy)(MobileClockUiNativeBridge* bridge);
};

using MobileClockUiNativeBridgeGetApi = const MobileClockUiNativeBridgeApi* (*)(
    std::uint32_t requestedAbiVersion);

extern "C" __declspec(dllexport) const MobileClockUiNativeBridgeApi* MobileClockUiNativeBridge_GetApi(
    std::uint32_t requestedAbiVersion);