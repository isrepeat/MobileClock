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
#include "../Session/PreviewSessionApi.h"
#include "../Session/PreviewSession.h"
#include "../Bridge/PreviewPluginBridge.h"
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

namespace AndroidAppPreviewerPluginSDK {
    xp_session* SessionApi::xp_create_session(
        int width,
        int height
    ) {
        try {
            xaml::bridge::lastError.clear();
            return mobileclock::preview::PreviewPluginApi::CreateSession(width, height);
        } catch (const std::exception& error) {
            xaml::bridge::lastError = error.what();
            return nullptr;
        }
    }

    void SessionApi::xp_destroy_session(xp_session* session) {
        mobileclock::preview::PreviewPluginApi::DestroySession(session);
    }

    int SessionApi::xp_session_load_page(
        xp_session* session,
        const char* page
    ) {
        try {
            xaml::bridge::lastError.clear();
            if (session == nullptr) {
                throw std::invalid_argument("Session is required");
            }
            return mobileclock::preview::PreviewSessionApi(*session).LoadPage(page) ? 1 : 0;
        } catch (const std::exception& error) {
            xaml::bridge::lastError = error.what();
            return 0;
        }
    }

    int SessionApi::xp_session_current_page(
        xp_session* session,
        char* page,
        int capacity
    ) {
        try {
            xaml::bridge::lastError.clear();
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
            xaml::bridge::lastError = error.what();
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
            xaml::bridge::lastError.clear();
            if (session == nullptr || target == nullptr) {
                throw std::invalid_argument("Session and target page are required");
            }
            LOG_INFO(
                "AndroidAppPreviewer.Route",
                "Native route request: current='{}', target='{}'",
                session->value.Session().CurrentPageName(),
                target);
            if (!session->value.Session().NavigatePreviewRoute(target, xaml::bridge::lastError)) {
                LOG_WARNING("AndroidAppPreviewer.Route", "Native route rejected: {}", xaml::bridge::lastError);
                return 0;
            }
            if (session->value.Session().CurrentPageName() != target) {
                xaml::bridge::lastError = std::format("Preview route did not reach {}", target);
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
            xaml::bridge::lastError = error.what();
            LOG_ERROR("AndroidAppPreviewer.Route", "Native route threw: {}", xaml::bridge::lastError);
            return 0;
        }
    }

    int SessionApi::xp_session_navigate_preview_route_path(
        xp_session* session,
        const char* path
    ) {
        try {
            xaml::bridge::lastError.clear();
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
            if (!session->value.Session().NavigatePreviewRoute(pages, xaml::bridge::lastError)) {
                LOG_WARNING("AndroidAppPreviewer.Route", "Native explicit route rejected: {}", xaml::bridge::lastError);
                return 0;
            }
            if (session->value.Session().CurrentPageName() != pages.back()) {
                xaml::bridge::lastError = std::format("Preview route did not reach {}", pages.back());
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
            xaml::bridge::lastError = error.what();
            LOG_ERROR("AndroidAppPreviewer.Route", "Native explicit route threw: {}", xaml::bridge::lastError);
            return 0;
        }
    }

    int SessionApi::xp_session_preview_route_graph(
        xp_session* session,
        char* graph,
        int capacity
    ) {
        try {
            xaml::bridge::lastError.clear();
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
            xaml::bridge::lastError = error.what();
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
            xaml::bridge::lastError.clear();
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
            xaml::bridge::lastError = error.what();
            return 0;
        }
    }

    int SessionApi::xp_session_apply_preview_scenario(
        xp_session* session,
        const char* page,
        const char* json
    ) {
        try {
            xaml::bridge::lastError.clear();
            if (session == nullptr) {
                throw std::invalid_argument("Session is required");
            }
            return mobileclock::preview::PreviewSessionApi(*session).ApplyScenario(page, json) ? 1 : 0;
        } catch (const std::exception& error) {
            xaml::bridge::lastError = error.what();
            return 0;
        }
    }

    int SessionApi::xp_session_export_preview_state(xp_session* session) {
        try {
            xaml::bridge::lastError.clear();
            if (session == nullptr) {
                throw std::invalid_argument("Session is required");
            }
            if (!session->value.ExportState()) {
                xaml::bridge::lastError = "Cannot persist preview state";
                return 0;
            }
            return 1;
        } catch (const std::exception& error) {
            xaml::bridge::lastError = error.what();
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
            xaml::bridge::lastError = error.what();
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
            xaml::bridge::lastError.clear();
            if (session == nullptr) {
                throw std::invalid_argument("Session is required");
            }
            return mobileclock::preview::PreviewSessionApi(*session).ReloadMarkup(page, markup, sourcePath) ? 1 : 0;
        } catch (const std::exception& error) {
            xaml::bridge::lastError = error.what();
            return 0;
        }
    }

    int SessionApi::xp_session_resize(
        xp_session* session,
        int width,
        int height
    ) {
        try {
            xaml::bridge::lastError.clear();
            if (session == nullptr || width <= 0 || height <= 0) {
                throw std::invalid_argument("Session and positive dimensions are required");
            }
            mobileclock::preview::_details::ClearInspectionWireframe(*session);
            session->value.Resize(width, height);
            return 1;
        } catch (const std::exception& error) {
            xaml::bridge::lastError = error.what();
            return 0;
        }
    }

    int SessionApi::xp_session_set_animation_playback_rate(
        xp_session* session,
        float value
    ) {
        try {
            xaml::bridge::lastError.clear();
            if (session == nullptr) {
                throw std::invalid_argument("Session is required");
            }
            session->value.Session().SetAnimationPlaybackRate(value);
            return 1;
        } catch (const std::exception& error) {
            xaml::bridge::lastError = error.what();
            return 0;
        }
    }

    int SessionApi::xp_session_set_status(
        xp_session* session,
        const char* value
    ) {
        try {
            xaml::bridge::lastError.clear();
            if (session == nullptr || value == nullptr) {
                throw std::invalid_argument("Session and status are required");
            }
            session->value.Controller().Dispatch(mobileclock::application::core::AppSessionSignal::setStatus, {value, {}});
            return 1;
        } catch (const std::exception& error) {
            xaml::bridge::lastError = error.what();
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
        return mobileclock::preview::PreviewSessionApi(*session).Inspect(x, y, *result) ? 1 : 0;
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
        return mobileclock::preview::PreviewSessionApi(*session).SetInspectionWireframe(
            thickness, lineStyle, color, marginColor, paddingColor) ? 1 : 0;
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
                mobileclock::preview::_details::ClearSelectedWireframe(*session);
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
        mobileclock::preview::_details::ClearInspectionWireframe(*session);
        return 1;
    }

    int SessionApi::xp_session_clear_selected_inspection_element(xp_session* session) {
        if (session == nullptr) {
            return 0;
        }
        mobileclock::preview::_details::ClearSelectedWireframe(*session);
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
        xaml::Element* const element = xaml::bridge::_details::FindElementAtSource(
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
        mobileclock::preview::_details::ClearSelectedWireframe(*session);
        mobileclock::preview::_details::SetSelectedWireframe(*session, *element);
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
            mobileclock::preview::_details::ClearInspectionWireframe(*session);
            return 0;
        }
        mobileclock::preview::_details::SetSelectedWireframe(*session, *session->inspectionElement);
        return 1;
    }

    int SessionApi::xp_session_update(xp_session* session) {
        if (session == nullptr) {
            return 0;
        }
        return mobileclock::preview::PreviewSessionApi(*session).Update() ? 1 : 0;
    }

    int SessionApi::xp_session_render_angle_surface(
        xp_session* session,
        xp_angle_surface* surface,
        unsigned char* destination,
        int destinationStride,
        int destinationCapacity
    ) {
        try {
            xaml::bridge::lastError.clear();
            if (session == nullptr || surface == nullptr) {
                throw std::invalid_argument("Session and surface are required");
            }
            return mobileclock::preview::PreviewSessionApi(*session).Render(
                *surface, destination, destinationStride, destinationCapacity) ? 1 : 0;
        } catch (const std::exception& error) {
            xaml::bridge::lastError = error.what();
            return 0;
        }
    }
} // namespace AndroidAppPreviewerPluginSDK