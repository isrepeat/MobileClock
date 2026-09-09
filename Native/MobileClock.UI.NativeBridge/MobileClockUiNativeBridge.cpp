#include "MobileClockUiNativeBridge.h"

namespace _details {
    constexpr std::uint32_t abiVersion = 3;

    MobileClockUiNativeBridge* Create();
    void Destroy(MobileClockUiNativeBridge* bridge);

    const MobileClockUiNativeBridgeApi api{
        abiVersion,
        Create,
        Destroy,
    };
}

struct MobileClockUiNativeBridge {
};

const MobileClockUiNativeBridgeApi* MobileClockUiNativeBridge_GetApi(
    std::uint32_t requestedAbiVersion) {
    if (requestedAbiVersion != _details::abiVersion) {
        return nullptr;
    }
    return &_details::api;
}

namespace _details {
    MobileClockUiNativeBridge* Create() {
        return new MobileClockUiNativeBridge();
    }

    void Destroy(MobileClockUiNativeBridge* bridge) {
        delete bridge;
    }

}