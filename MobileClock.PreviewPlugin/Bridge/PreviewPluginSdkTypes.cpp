#include "PreviewPluginSdkTypes.h"

namespace AndroidAppPreviewerPluginSDK {
    xp_angle_surface::xp_angle_surface(
        int width,
        int height,
        const char* fontPath,
        const char* resourceRoot)
        : width(width)
        , height(height)
        , value(width, height, fontPath, resourceRoot) {
    }

    xp_session::xp_session(int width, int height)
        : value(width, height) {
    }
}