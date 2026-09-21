#include "PreviewSessionApi.h"

#include <XamlRuntime/XamlLayout.h>

#include <stdexcept>
#include <cstring>

namespace mobileclock::preview {
    PreviewSessionApi::PreviewSessionApi(AndroidAppPreviewerPluginSDK::xp_session& session)
        : session(session) {
    }

    bool PreviewSessionApi::LoadPage(const char* page) {
        if (page == nullptr) {
            throw std::invalid_argument("Page is required");
        }
        _details::ClearInspectionWireframe(this->session);
        _details::ClearSelectedWireframe(this->session);
        if (!this->session.value.Session().LoadPage(page)) {
            throw std::invalid_argument("Unknown MobileClock page");
        }
        return true;
    }

    bool PreviewSessionApi::ApplyScenario(const char* page, const char* json) {
        if (page == nullptr || json == nullptr) {
            throw std::invalid_argument("Page and scenario are required");
        }
        _details::ClearInspectionWireframe(this->session);
        _details::ClearSelectedWireframe(this->session);
        if (!this->session.value.Session().ApplyPreviewScenario(page, json, xaml::bridge::lastError)) {
            if (xaml::bridge::lastError.empty()) {
                xaml::bridge::lastError = "Preview scenario was not applied";
            }
            return false;
        }
        return true;
    }

    bool PreviewSessionApi::ReloadMarkup(const char* page, const char* markup, const char* sourcePath) {
        if (page == nullptr || markup == nullptr || sourcePath == nullptr) {
            throw std::invalid_argument("Page, markup and source path are required");
        }
        _details::ClearInspectionWireframe(this->session);
        _details::ClearSelectedWireframe(this->session);
        return this->session.value.Session().ReloadMarkup(page, markup, sourcePath, xaml::bridge::lastError);
    }

    bool PreviewSessionApi::Inspect(
        float x,
        float y,
        AndroidAppPreviewerPluginSDK::xp_session_inspection_result& result) {
        xaml::Element* element = xaml::bridge::_details::HitTestVisual(
            this->session.value.Session().Root(), x, y);
        while (element != nullptr && element->SourceLine() <= 0) {
            element = element->Parent();
        }
        if (element == nullptr) {
            _details::ClearInspectionWireframe(this->session);
            return false;
        }
        _details::SetInspectionWireframe(this->session, *element);
        const xaml::Rect bounds = element->Bounds();
        result = {element->SourceLine(), element->SourceColumn()};
        std::strncpy(result.sourcePath, element->SourcePath().c_str(), sizeof(result.sourcePath) - 1);
        result.sourcePath[sizeof(result.sourcePath) - 1] = '\0';
        result.bounds = {bounds.x, bounds.y, bounds.width, bounds.height};
        return true;
    }

    bool PreviewSessionApi::SetInspectionWireframe(
        float thickness,
        int lineStyle,
        AndroidAppPreviewerPluginSDK::xp_color color,
        AndroidAppPreviewerPluginSDK::xp_color marginColor,
        AndroidAppPreviewerPluginSDK::xp_color paddingColor) {
        if (thickness <= 0.0f || (lineStyle != 0 && lineStyle != 1)) {
            return false;
        }
        this->session.inspectionWireframe = {
            thickness,
            lineStyle == 0 ? xaml::attr::WireframeLineStyle::solid : xaml::attr::WireframeLineStyle::dashed,
            {color.red, color.green, color.blue, color.alpha},
            {marginColor.red, marginColor.green, marginColor.blue, marginColor.alpha},
            {paddingColor.red, paddingColor.green, paddingColor.blue, paddingColor.alpha},
        };
        if (this->session.inspectionElement != nullptr) {
            if (this->session.inspectionElementLifetime.expired()) {
                _details::ClearInspectionWireframe(this->session);
            } else {
                this->session.inspectionElement->SetInspectionWireframe(this->session.inspectionWireframe);
            }
        }
        return true;
    }

    bool PreviewSessionApi::Update() {
        this->session.value.Session().Update();
        return true;
    }

    bool PreviewSessionApi::Render(
        AndroidAppPreviewerPluginSDK::xp_angle_surface& surface,
        unsigned char* destination,
        int destinationStride,
        int destinationCapacity) {
        if (destination == nullptr || destinationStride < surface.width * 4
            || destinationCapacity / destinationStride < surface.height) {
            throw std::invalid_argument("Invalid MobileClock ANGLE render arguments");
        }
        surface.value.Render(this->session.value.Session(), destination, destinationStride);
        return true;
    }
}