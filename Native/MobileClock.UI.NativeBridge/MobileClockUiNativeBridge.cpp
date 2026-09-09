#include "MobileClockUiNativeBridge.h"

#include <XamlRuntime/Animation.h>

#include "MobileClock.UI/Controls/ControlsRuntime.h"

namespace _details {
    constexpr std::uint32_t abiVersion = 1;

    MobileClockUiNativeBridge* Create();
    void Destroy(MobileClockUiNativeBridge* bridge);
    void Attach(MobileClockUiNativeBridge* bridge, void* root, const char* className);
    void Detach(MobileClockUiNativeBridge* bridge, void* root);

    MobileClockUiRebuildState* CaptureRebuildState(MobileClockUiNativeBridge* bridge, void* target);

    int RestoreRebuildState(
        MobileClockUiNativeBridge* bridge,
        MobileClockUiRebuildState* state,
        void* pageRoot,
        void* animations);

    void DestroyRebuildState(MobileClockUiRebuildState* state);

    const MobileClockUiNativeBridgeApi api{
        abiVersion,
        Create,
        Destroy,
        Attach,
        Detach,
        CaptureRebuildState,
        RestoreRebuildState,
        DestroyRebuildState,
    };
}

struct MobileClockUiNativeBridge {
    mobileclock::ui::controls::ControlsRuntime value;
};

struct MobileClockUiRebuildState {
    std::unique_ptr<mobileclock::ui::controls::ControlsRuntime::RebuildState> value;
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

    void Attach(
        MobileClockUiNativeBridge* bridge,
        void* root,
        const char* className) {
        if (bridge != nullptr && root != nullptr && className != nullptr) {
            bridge->value.Attach(*static_cast<xaml::Element*>(root), className);
        }
    }

    void Detach(MobileClockUiNativeBridge* bridge, void* root) {
        if (bridge != nullptr && root != nullptr) {
            bridge->value.Detach(*static_cast<xaml::Element*>(root));
        }
    }

    MobileClockUiRebuildState* CaptureRebuildState(
        MobileClockUiNativeBridge* bridge,
        void* target) {
        if (bridge == nullptr || target == nullptr) {
            return nullptr;
        }
        auto value = bridge->value.CaptureRebuildState(*static_cast<xaml::Element*>(target));
        if (!value) {
            return nullptr;
        }
        return new MobileClockUiRebuildState{std::move(value)};
    }

    int RestoreRebuildState(
        MobileClockUiNativeBridge* bridge,
        MobileClockUiRebuildState* state,
        void* pageRoot,
        void* animations) {
        if (bridge == nullptr || state == nullptr || pageRoot == nullptr || animations == nullptr) {
            return 0;
        }
        return bridge->value.RestoreRebuildState(
            *state->value,
            *static_cast<xaml::Element*>(pageRoot),
            *static_cast<xaml::AnimationController*>(animations)) ? 1 : 0;
    }

    void DestroyRebuildState(MobileClockUiRebuildState* state) {
        delete state;
    }
}
