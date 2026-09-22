#pragma once
#include "../Bridge/PreviewPluginSdkTypes.h"

namespace mobileclock::preview::session {
    //
    // Управляет временным состоянием inspection и selection одного preview-сеанса.
    //
    class PreviewSessionInspection final {
    public:
        PreviewSessionInspection() = delete;

        static void ClearInspectionWireframe(AndroidAppPreviewerPluginSDK::xp_session& session);
        static void ClearSelectedWireframe(AndroidAppPreviewerPluginSDK::xp_session& session);
        static void SetInspectionWireframe(
            AndroidAppPreviewerPluginSDK::xp_session& session,
            xaml::Element& element);
        static void SetSelectedWireframe(
            AndroidAppPreviewerPluginSDK::xp_session& session,
            xaml::Element& element);
    };
}