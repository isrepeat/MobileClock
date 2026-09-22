#include "SessionApi.h"

#include <AndroidAppPreviewer.PluginSDK/AndroidAppPreviewerPlugin.h>
#include <Helpers.Logging/Logging.h>
#include <HelpersNew/Geometry/ContainsPoint.h>
#include <XamlRuntime/InteractionController.h>
#include <XamlRuntime/ScrollController.h>
#include <XamlRuntime/ElementBuilder.h>
#include <XamlRuntime/RenderEngine.h>
#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Animation.h>
#include <XamlRuntime/Input.h>

#include "MobileClock.Presentation/Core/PreviewSession.h"

#include "../Rendering/AngleRenderSurface.h"
#include "../Rendering/RecordingBackend.h"
#include "../Session/PreviewNavigationController.h"
#include "../Session/PreviewSession.h"
#include "../Bridge/Diagnostic.h"
#include "../Bridge/ElementTree.h"
#include "../Bridge/PreviewPluginSdkTypes.h"
#include "../Bridge/TextBuffer.h"
#include "PreviewPluginApi.h"

#include <unordered_map>
#include <string_view>
#include <filesystem>
#include <functional>
#include <algorithm>
#include <stdexcept>
#include <fstream>
#include <cstring>
#include <cctype>
#include <chrono>
#include <format>
#include <memory>
#include <string>
#include <vector>
#include <cmath>

namespace mobileclock::preview::api {
    using namespace AndroidAppPreviewerPluginSDK;

    //
    // API
    //
    xp_session* SessionApi::xp_create_session(
        int width,
        int height
    ) {
        try {
            mobileclock::preview::bridge::LastError().clear();
            return mobileclock::preview::api::PreviewPluginApi::CreateSession(width, height);
        } catch (const std::exception& error) {
            mobileclock::preview::bridge::LastError() = error.what();
            return nullptr;
        }
    }

    void SessionApi::xp_destroy_session(xp_session* session) {
        mobileclock::preview::api::PreviewPluginApi::DestroySession(session);
    }

    int SessionApi::xp_session_load_page(
        xp_session* session,
        const char* page
    ) {
        try {
            mobileclock::preview::bridge::LastError().clear();
            if (session == nullptr) {
                throw std::invalid_argument("Session is required");
            }
            return LoadPage(*session, page) ? 1 : 0;
        } catch (const std::exception& error) {
            mobileclock::preview::bridge::LastError() = error.what();
            return 0;
        }
    }

    int SessionApi::xp_session_current_page(
        xp_session* session,
        char* page,
        int capacity
    ) {
        try {
            mobileclock::preview::bridge::LastError().clear();
            if (session == nullptr || page == nullptr || capacity <= 0) {
                throw std::invalid_argument("Session, page buffer and positive capacity are required");
            }
            const std::string_view name = session->value.Session().CurrentPageName();
            mobileclock::preview::bridge::TextBuffer::Write(
                name,
                page,
                static_cast<size_t>(capacity),
                "Page buffer is too small");
            return 1;
        } catch (const std::exception& error) {
            mobileclock::preview::bridge::LastError() = error.what();
            return 0;
        }
    }

    int SessionApi::xp_session_is_transitioning(xp_session* session) {
        if (session == nullptr) {
            return 0;
        }
        return session->value.Session().IsTransitioning() ? 1 : 0;
    }

    int SessionApi::xp_session_navigate_preview_route(
        xp_session* session,
        const char* target
    ) {
        try {
            mobileclock::preview::bridge::LastError().clear();
            if (session == nullptr || target == nullptr) {
                throw std::invalid_argument("Session and target page are required");
            }
            LOG_INFO(
                "AndroidAppPreviewer.Route",
                "Native route request: current='{}', target='{}'",
                session->value.Session().CurrentPageName(),
                target);
            if (!session->value.Session().NavigatePreviewRoute(target, mobileclock::preview::bridge::LastError())) {
                LOG_WARNING("AndroidAppPreviewer.Route", "Native route rejected: {}", mobileclock::preview::bridge::LastError());
                return 0;
            }
            if (session->value.Session().CurrentPageName() != target) {
                mobileclock::preview::bridge::LastError() = std::format("Preview route did not reach {}", target);
                LOG_ERROR(
                    "AndroidAppPreviewer.Route",
                    "Native route failed after pending actions: target='{}', actual='{}'",
                    target,
                    session->value.Session().CurrentPageName());
                return 0;
            }
            LOG_INFO("AndroidAppPreviewer.Route", "Native route completed: active='{}'", session->value.Session().CurrentPageName());
            return 1;
        } catch (const std::exception& error) {
            mobileclock::preview::bridge::LastError() = error.what();
            LOG_ERROR("AndroidAppPreviewer.Route", "Native route threw: {}", mobileclock::preview::bridge::LastError());
            return 0;
        }
    }

    int SessionApi::xp_session_navigate_preview_route_path(
        xp_session* session,
        const char* path
    ) {
        try {
            mobileclock::preview::bridge::LastError().clear();
            if (session == nullptr || path == nullptr) {
                throw std::invalid_argument("Session and route path are required");
            }
            LOG_INFO(
                "AndroidAppPreviewer.Route",
                "Native explicit route request: current='{}', path='{}'",
                session->value.Session().CurrentPageName(),
                path);
            std::vector<std::string_view> pages;
            const std::string_view serialized(path);
            size_t start = 0;
            while (start <= serialized.size()) {
                const size_t separator = serialized.find('>', start);
                const std::string_view page = serialized.substr(start, separator - start);
                if (page.empty()) {
                    throw std::invalid_argument("Preview route contains an empty page");
                }
                pages.push_back(page);
                if (separator == std::string_view::npos) {
                    break;
                }
                start = separator + 1;
            }
            if (!session->value.Session().NavigatePreviewRoute(pages, mobileclock::preview::bridge::LastError())) {
                LOG_WARNING("AndroidAppPreviewer.Route", "Native explicit route rejected: {}", mobileclock::preview::bridge::LastError());
                return 0;
            }
            if (session->value.Session().CurrentPageName() != pages.back()) {
                mobileclock::preview::bridge::LastError() = std::format("Preview route did not reach {}", pages.back());
                LOG_ERROR(
                    "AndroidAppPreviewer.Route",
                    "Native explicit route failed after pending actions: target='{}', actual='{}'",
                    pages.back(),
                    session->value.Session().CurrentPageName());
                return 0;
            }
            LOG_INFO(
                "AndroidAppPreviewer.Route",
                "Native explicit route completed: active='{}'",
                session->value.Session().CurrentPageName());
            return 1;
        } catch (const std::exception& error) {
            mobileclock::preview::bridge::LastError() = error.what();
            LOG_ERROR("AndroidAppPreviewer.Route", "Native explicit route threw: {}", mobileclock::preview::bridge::LastError());
            return 0;
        }
    }

    int SessionApi::xp_session_preview_route_graph(
        xp_session* session,
        char* graph,
        int capacity
    ) {
        try {
            mobileclock::preview::bridge::LastError().clear();
            if (session == nullptr || graph == nullptr || capacity <= 0) {
                throw std::invalid_argument("Session, graph buffer and positive capacity are required");
            }
            const std::string value = session->value.Session().PreviewRouteGraph();
            mobileclock::preview::bridge::TextBuffer::Write(
                value,
                graph,
                static_cast<size_t>(capacity),
                "Preview route graph buffer is too small");
            return 1;
        } catch (const std::exception& error) {
            mobileclock::preview::bridge::LastError() = error.what();
            return 0;
        }
    }

    int SessionApi::xp_session_preview_page_title(
        xp_session* session,
        const char* page,
        char* title,
        int capacity
    ) {
        try {
            mobileclock::preview::bridge::LastError().clear();
            if (session == nullptr || page == nullptr || title == nullptr || capacity <= 0) {
                throw std::invalid_argument("Session, page, title buffer and positive capacity are required");
            }
            const std::string_view value = session->value.Session().PreviewPageTitle(page);
            mobileclock::preview::bridge::TextBuffer::Write(
                value,
                title,
                static_cast<size_t>(capacity),
                "Preview page title buffer is too small");
            return 1;
        } catch (const std::exception& error) {
            mobileclock::preview::bridge::LastError() = error.what();
            return 0;
        }
    }

    int SessionApi::xp_session_apply_preview_scenario(
        xp_session* session,
        const char* page,
        const char* json
    ) {
        try {
            mobileclock::preview::bridge::LastError().clear();
            if (session == nullptr) {
                throw std::invalid_argument("Session is required");
            }
            return ApplyScenario(*session, page, json) ? 1 : 0;
        } catch (const std::exception& error) {
            mobileclock::preview::bridge::LastError() = error.what();
            return 0;
        }
    }

    int SessionApi::xp_session_export_preview_state(xp_session* session) {
        try {
            mobileclock::preview::bridge::LastError().clear();
            if (session == nullptr) {
                throw std::invalid_argument("Session is required");
            }
            if (!session->value.ExportState()) {
                mobileclock::preview::bridge::LastError() = "Cannot persist preview state";
                return 0;
            }
            return 1;
        } catch (const std::exception& error) {
            mobileclock::preview::bridge::LastError() = error.what();
            return 0;
        }
    }

    int SessionApi::xp_session_can_save_preview_state(xp_session* session) {
        try {
            if (session == nullptr) {
                throw std::invalid_argument("Session is required");
            }
            return session->value.CanSaveState() ? 1 : 0;
        } catch (const std::exception& error) {
            mobileclock::preview::bridge::LastError() = error.what();
            return 0;
        }
    }

    int SessionApi::xp_session_reload_markup(
        xp_session* session,
        const char* page,
        const char* markup,
        const char* sourcePath
    ) {
        try {
            mobileclock::preview::bridge::LastError().clear();
            if (session == nullptr) {
                throw std::invalid_argument("Session is required");
            }
            return ReloadMarkup(*session, page, markup, sourcePath) ? 1 : 0;
        } catch (const std::exception& error) {
            mobileclock::preview::bridge::LastError() = error.what();
            return 0;
        }
    }

    int SessionApi::xp_session_resize(
        xp_session* session,
        int width,
        int height
    ) {
        try {
            mobileclock::preview::bridge::LastError().clear();
            if (session == nullptr || width <= 0 || height <= 0) {
                throw std::invalid_argument("Session and positive dimensions are required");
            }
            ClearInspectionWireframe(*session);
            session->value.Resize(width, height);
            return 1;
        } catch (const std::exception& error) {
            mobileclock::preview::bridge::LastError() = error.what();
            return 0;
        }
    }

    int SessionApi::xp_session_set_animation_playback_rate(
        xp_session* session,
        float value
    ) {
        try {
            mobileclock::preview::bridge::LastError().clear();
            if (session == nullptr) {
                throw std::invalid_argument("Session is required");
            }
            session->value.Session().SetAnimationPlaybackRate(value);
            return 1;
        } catch (const std::exception& error) {
            mobileclock::preview::bridge::LastError() = error.what();
            return 0;
        }
    }

    int SessionApi::xp_session_set_status(
        xp_session* session,
        const char* value
    ) {
        try {
            mobileclock::preview::bridge::LastError().clear();
            if (session == nullptr || value == nullptr) {
                throw std::invalid_argument("Session and status are required");
            }
            session->value.Controller().Dispatch(mobileclock::application::core::AppSessionSignal::setStatus, {value, {}});
            return 1;
        } catch (const std::exception& error) {
            mobileclock::preview::bridge::LastError() = error.what();
            return 0;
        }
    }

    int SessionApi::xp_session_pointer_down(
        xp_session* session,
        float x,
        float y
    ) {
        if (session == nullptr) {
            return 0;
        }
        session->value.Session().PointerDown(x, y);
        return 1;
    }

    int SessionApi::xp_session_pointer_move(
        xp_session* session,
        float x,
        float y
    ) {
        if (session == nullptr) {
            return 0;
        }
        session->value.Session().PointerMove(x, y);
        return 1;
    }

    int SessionApi::xp_session_pointer_up(
        xp_session* session,
        float x,
        float y
    ) {
        if (session == nullptr) {
            return 0;
        }
        session->value.Session().PointerUp(x, y);
        return 1;
    }

    int SessionApi::xp_session_pointer_cancel(xp_session* session) {
        if (session == nullptr) {
            return 0;
        }
        session->value.Session().CancelPointer();
        return 1;
    }

    int SessionApi::xp_session_cursor_kind(
        xp_session* session,
        float x,
        float y
    ) {
        if (session == nullptr) {
            return 0;
        }
        xaml::Element& root = session->value.Session().Root();
        xaml::Element* const visual = xaml::HitTestVisual(root, x, y);
        if (visual == nullptr) {
            return 0;
        }
        xaml::Element* const interactive = xaml::HitTest(root, x, y);
        if (interactive != nullptr && interactive->Type() != xaml::ElementType::scrollViewer) {
            return 1;
        }
        for (xaml::Element* element = visual; element != nullptr; element = element->Parent()) {
            if (element->Type() == xaml::ElementType::scrollViewer) {
                return 2;
            }
        }
        return 0;
    }

    int SessionApi::xp_session_inspect(
        xp_session* session,
        float x,
        float y,
        xp_session_inspection_result* result
    ) {
        if (session == nullptr || result == nullptr) {
            return 0;
        }
        return Inspect(*session, x, y, *result) ? 1 : 0;
    }

    int SessionApi::xp_session_set_inspection_wireframe(
        xp_session* session,
        float thickness,
        int lineStyle,
        xp_color color,
        xp_color marginColor,
        xp_color paddingColor
    ) {
        if (session == nullptr) {
            return 0;
        }
        return SetInspectionWireframe(
            *session,
            thickness,
            lineStyle,
            color,
            marginColor,
            paddingColor) ? 1 : 0;
    }

    int SessionApi::xp_session_set_selected_wireframe(
        xp_session* session,
        float thickness,
        int lineStyle,
        xp_color color,
        xp_color marginColor,
        xp_color paddingColor
    ) {
        if (session == nullptr || thickness <= 0.0f || (lineStyle != 0 && lineStyle != 1)) {
            return 0;
        }
        session->selectedWireframe = {
            thickness,
            lineStyle == 0 ? xaml::attr::WireframeLineStyle::solid : xaml::attr::WireframeLineStyle::dashed,
            {color.red, color.green, color.blue, color.alpha},
            {marginColor.red, marginColor.green, marginColor.blue, marginColor.alpha},
            {paddingColor.red, paddingColor.green, paddingColor.blue, paddingColor.alpha},
        };
        if (session->selectedElement != nullptr) {
            if (session->selectedElementLifetime.expired()) {
                ClearSelectedWireframe(*session);
            } else {
                session->selectedElement->SetSelectedWireframe(session->selectedWireframe);
            }
        }
        return 1;
    }

    int SessionApi::xp_session_clear_inspection_wireframe(xp_session* session) {
        if (session == nullptr) {
            return 0;
        }
        ClearInspectionWireframe(*session);
        return 1;
    }

    int SessionApi::xp_session_clear_selected_inspection_element(xp_session* session) {
        if (session == nullptr) {
            return 0;
        }
        ClearSelectedWireframe(*session);
        return 1;
    }

    int SessionApi::xp_session_select_inspection_element(
        xp_session* session,
        const char* sourcePath,
        int line,
        int column
    ) {
        if (session == nullptr || sourcePath == nullptr || line <= 0 || column <= 0) {
            return 0;
        }
        LOG_INFO(
            "AndroidAppPreviewer.Inspection",
            "Source selection requested at {}:{} in {}",
            line,
            column,
            sourcePath);
        xaml::Element* const element = mobileclock::preview::bridge::ElementTree::FindElementAtSource(
            session->value.Session().Root(),
            sourcePath,
            line,
            column);
        if (element == nullptr) {
            LOG_INFO("AndroidAppPreviewer.Inspection", "Source selection found no element");
            return 0;
        }
        // Не стираем текущий выбор, пока новая позиция редактора не сопоставлена
        // с элементом preview: перевод фокуса после клика меняет caret.
        ClearSelectedWireframe(*session);
        SetSelectedWireframe(*session, *element);
        const xaml::Rect& bounds = element->Bounds();
        LOG_INFO(
            "AndroidAppPreviewer.Inspection",
            "Source selection resolved to {}:{} id='{}' bounds=({}, {}, {}, {})",
            element->SourceLine(),
            element->SourceColumn(),
            element->Id(),
            bounds.x,
            bounds.y,
            bounds.width,
            bounds.height);
        return 1;
    }

    int SessionApi::xp_session_pin_inspection_element(xp_session* session) {
        if (session == nullptr || session->inspectionElement == nullptr) {
            return 0;
        }
        if (session->inspectionElementLifetime.expired()) {
            ClearInspectionWireframe(*session);
            return 0;
        }
        SetSelectedWireframe(*session, *session->inspectionElement);
        return 1;
    }

    int SessionApi::xp_session_update(xp_session* session) {
        if (session == nullptr) {
            return 0;
        }
        return Update(*session) ? 1 : 0;
    }

    int SessionApi::xp_session_render_angle_surface(
        xp_session* session,
        xp_angle_surface* surface,
        unsigned char* destination,
        int destinationStride,
        int destinationCapacity
    ) {
        try {
            mobileclock::preview::bridge::LastError().clear();
            if (session == nullptr || surface == nullptr) {
                throw std::invalid_argument("Session and surface are required");
            }
            return Render(
                *session,
                *surface,
                destination,
                destinationStride,
                destinationCapacity) ? 1 : 0;
        } catch (const std::exception& error) {
            mobileclock::preview::bridge::LastError() = error.what();
            return 0;
        }
    }

    //
    // Internal
    //
    bool SessionApi::LoadPage(
        xp_session& session,
        const char* page) {
        if (page == nullptr) {
            throw std::invalid_argument("Page is required");
        }
        ClearInspectionWireframe(session);
        ClearSelectedWireframe(session);
        if (!session.value.Session().LoadPage(page)) {
            throw std::invalid_argument("Unknown MobileClock page");
        }
        return true;
    }

    bool SessionApi::ApplyScenario(
        xp_session& session,
        const char* page,
        const char* json) {
        if (page == nullptr || json == nullptr) {
            throw std::invalid_argument("Page and scenario are required");
        }
        ClearInspectionWireframe(session);
        ClearSelectedWireframe(session);
        if (!session.value.Session().ApplyPreviewScenario(page, json, mobileclock::preview::bridge::LastError())) {
            if (mobileclock::preview::bridge::LastError().empty()) {
                mobileclock::preview::bridge::LastError() = "Preview scenario was not applied";
            }
            return false;
        }
        return true;
    }

    bool SessionApi::ReloadMarkup(
        xp_session& session,
        const char* page,
        const char* markup,
        const char* sourcePath) {
        if (page == nullptr || markup == nullptr || sourcePath == nullptr) {
            throw std::invalid_argument("Page, markup and source path are required");
        }
        ClearInspectionWireframe(session);
        ClearSelectedWireframe(session);
        return session.value.Session().ReloadMarkup(page, markup, sourcePath, mobileclock::preview::bridge::LastError());
    }

    bool SessionApi::Inspect(
        xp_session& session,
        float x,
        float y,
        xp_session_inspection_result& result) {
        xaml::Element* element = mobileclock::preview::bridge::ElementTree::HitTestVisual(
            session.value.Session().Root(), x, y);
        while (element != nullptr && element->SourceLine() <= 0) {
            element = element->Parent();
        }
        if (element == nullptr) {
            ClearInspectionWireframe(session);
            return false;
        }
        SetInspectionWireframe(session, *element);
        const xaml::Rect bounds = element->Bounds();
        result = { element->SourceLine(), element->SourceColumn() };
        std::strncpy(result.sourcePath, element->SourcePath().c_str(), sizeof(result.sourcePath) - 1);
        result.sourcePath[sizeof(result.sourcePath) - 1] = '\0';
        result.bounds = { bounds.x, bounds.y, bounds.width, bounds.height };
        return true;
    }

    bool SessionApi::SetInspectionWireframe(
        xp_session& session,
        float thickness,
        int lineStyle,
        xp_color color,
        xp_color marginColor,
        xp_color paddingColor) {
        if (thickness <= 0.0f || (lineStyle != 0 && lineStyle != 1)) {
            return false;
        }
        session.inspectionWireframe = {
            thickness,
            lineStyle == 0 ? xaml::attr::WireframeLineStyle::solid : xaml::attr::WireframeLineStyle::dashed,
            {color.red, color.green, color.blue, color.alpha},
            {marginColor.red, marginColor.green, marginColor.blue, marginColor.alpha},
            {paddingColor.red, paddingColor.green, paddingColor.blue, paddingColor.alpha},
        };
        if (session.inspectionElement != nullptr) {
            if (session.inspectionElementLifetime.expired()) {
                ClearInspectionWireframe(session);
            }
            else {
                session.inspectionElement->SetInspectionWireframe(session.inspectionWireframe);
            }
        }
        return true;
    }

    bool SessionApi::Update(xp_session& session) {
        session.value.Session().Update();
        return true;
    }

    bool SessionApi::Render(
        xp_session& session,
        xp_angle_surface& surface,
        unsigned char* destination,
        int destinationStride,
        int destinationCapacity) {
        if (destination == nullptr || destinationStride < surface.width * 4
            || destinationCapacity / destinationStride < surface.height) {
            throw std::invalid_argument("Invalid MobileClock ANGLE render arguments");
        }
        surface.value.Render(session.value.Session(), destination, destinationStride);
        return true;
    }

    void SessionApi::ClearInspectionWireframe(xp_session& session) {
        if (session.inspectionElement != nullptr && !session.inspectionElementLifetime.expired()) {
            session.inspectionElement->ClearInspectionWireframe();
        }
        session.inspectionElement = nullptr;
        session.inspectionElementLifetime.reset();
    }

    void SessionApi::ClearSelectedWireframe(xp_session& session) {
        if (session.selectedElement != nullptr && !session.selectedElementLifetime.expired()) {
            session.selectedElement->ClearSelectedWireframe();
        }
        session.selectedElement = nullptr;
        session.selectedElementLifetime.reset();
    }

    void SessionApi::SetInspectionWireframe(
        xp_session& session,
        xaml::Element& element) {
        if (session.inspectionElement != &element) {
            ClearInspectionWireframe(session);
            session.inspectionElement = &element;
            session.inspectionElementLifetime = element.LifetimeToken();
        }
        element.SetInspectionWireframe(session.inspectionWireframe);
    }

    void SessionApi::SetSelectedWireframe(
        xp_session& session,
        xaml::Element& element) {
        if (session.selectedElement != &element) {
            ClearSelectedWireframe(session);
            session.selectedElement = &element;
            session.selectedElementLifetime = element.LifetimeToken();
        }
        element.SetSelectedWireframe(session.selectedWireframe);
    }
} // namespace mobileclock::preview::api