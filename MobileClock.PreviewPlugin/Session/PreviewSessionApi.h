#pragma once
#include "../Bridge/PreviewPluginSdkTypes.h"

namespace mobileclock::preview::session {
    //
    // Объектный фасад операций одного preview-сеанса поверх C ABI handle.
    //
    class PreviewSessionApi final {
    public:
        explicit PreviewSessionApi(AndroidAppPreviewerPluginSDK::xp_session& session);

        bool LoadPage(const char* page);
        bool ApplyScenario(const char* page, const char* json);
        bool ReloadMarkup(const char* page, const char* markup, const char* sourcePath);
        bool Inspect(float x, float y, AndroidAppPreviewerPluginSDK::xp_session_inspection_result& result);
        bool SetInspectionWireframe(
            float thickness,
            int lineStyle,
            AndroidAppPreviewerPluginSDK::xp_color color,
            AndroidAppPreviewerPluginSDK::xp_color marginColor,
            AndroidAppPreviewerPluginSDK::xp_color paddingColor);
        bool Update();
        bool Render(
            AndroidAppPreviewerPluginSDK::xp_angle_surface& surface,
            unsigned char* destination,
            int destinationStride,
            int destinationCapacity);

    private:
        AndroidAppPreviewerPluginSDK::xp_session& session;
    };
}