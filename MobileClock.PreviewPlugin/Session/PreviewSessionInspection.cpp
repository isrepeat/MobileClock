#include "PreviewSessionInspection.h"

namespace mobileclock::preview::session {
    void PreviewSessionInspection::ClearInspectionWireframe(AndroidAppPreviewerPluginSDK::xp_session& session) {
        if (session.inspectionElement != nullptr && !session.inspectionElementLifetime.expired()) {
            session.inspectionElement->ClearInspectionWireframe();
        }
        session.inspectionElement = nullptr;
        session.inspectionElementLifetime.reset();
    }

    void PreviewSessionInspection::ClearSelectedWireframe(AndroidAppPreviewerPluginSDK::xp_session& session) {
        if (session.selectedElement != nullptr && !session.selectedElementLifetime.expired()) {
            session.selectedElement->ClearSelectedWireframe();
        }
        session.selectedElement = nullptr;
        session.selectedElementLifetime.reset();
    }

    void PreviewSessionInspection::SetInspectionWireframe(
        AndroidAppPreviewerPluginSDK::xp_session& session,
        xaml::Element& element) {
        if (session.inspectionElement != &element) {
            ClearInspectionWireframe(session);
            session.inspectionElement = &element;
            session.inspectionElementLifetime = element.LifetimeToken();
        }
        element.SetInspectionWireframe(session.inspectionWireframe);
    }

    void PreviewSessionInspection::SetSelectedWireframe(
        AndroidAppPreviewerPluginSDK::xp_session& session,
        xaml::Element& element) {
        if (session.selectedElement != &element) {
            ClearSelectedWireframe(session);
            session.selectedElement = &element;
            session.selectedElementLifetime = element.LifetimeToken();
        }
        element.SetSelectedWireframe(session.selectedWireframe);
    }
}