#include <AndroidAppPreviewer.PluginSDK/AndroidAppPreviewerPlugin.h>
#include <XamlRuntime/InteractionController.h>
#include <XamlRuntime/ScrollController.h>
#include <XamlRuntime/ElementBuilder.h>
#include <XamlRuntime/RenderEngine.h>
#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Animation.h>
#include <XamlRuntime/Input.h>

#include <Helpers.Logging/Logging.h>
#include <HelpersNew/Geometry/ContainsPoint.h>

#include "MobileClock.Presentation/Core/PreviewSession.h"

#include "Rendering/AngleRenderSurface.h"
#include "Rendering/RecordingBackend.h"
#include "Session/PreviewNavigationController.h"
#include "Session/PreviewSessionApi.h"
#include "Session/PreviewSession.h"
#include "Bridge/PreviewPluginBridge.h"
#include "Bridge/TextBuffer.h"
#include "Api/PreviewPluginApi.h"

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
    extern "C" {
        const char* xp_last_error(void) {
            return mobileclock::preview::PreviewPluginApi::LastError();
        }

        uint32_t xp_get_abi_version(void) {
            return mobileclock::preview::PreviewPluginApi::AbiVersion();
        }

        const char* xp_get_last_error(void) {
            return AndroidAppPreviewerPluginSDK::xp_last_error();
        }

        int xp_get_plugin_info(char* pluginInfoJson, int capacity) {
            try {
                xaml::bridge::lastError.clear();
                return mobileclock::preview::PreviewPluginApi::WritePluginInfo(pluginInfoJson, capacity) ? 1 : 0;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        int xp_get_initial_page_id(void* session, char* pageId, int capacity) {
            return AndroidAppPreviewerPluginSDK::xp_session_current_page(static_cast<xp_session*>(session), pageId, capacity);
        }

        int xp_get_navigation_graph(void* session, char* graphJson, int capacity) {
            try {
                xaml::bridge::lastError.clear();
                if (session == nullptr || graphJson == nullptr || capacity <= 0) {
                    throw std::invalid_argument("Session, graph buffer and positive capacity are required");
                }
                const std::string json = static_cast<xp_session*>(session)->value.Navigation().BuildGraphJson();
                mobileclock::preview::bridge::TextBuffer::Write(
                    json,
                    graphJson,
                    static_cast<size_t>(capacity),
                    "Navigation graph buffer is too small");
                return 1;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        int xp_navigate(void* session, const char* navigationRequestJson) {
            try {
                xaml::bridge::lastError.clear();
                if (session == nullptr || navigationRequestJson == nullptr) {
                    throw std::invalid_argument("Session and navigation request are required");
                }
                const std::vector<std::string> ids = mobileclock::preview::PreviewSession::ParseNavigationTransitionIds(navigationRequestJson);
                std::vector<std::string_view> transitionIds;
                transitionIds.reserve(ids.size());
                for (const std::string& id : ids) {
                    transitionIds.push_back(id);
                }
                if (!static_cast<xp_session*>(session)->value.Navigation().Navigate(
                    transitionIds,
                    xaml::bridge::lastError)) {
                    return 0;
                }
                return 1;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        xp_session* xp_create_session(int width, int height) {
            try {
                xaml::bridge::lastError.clear();
                return mobileclock::preview::PreviewPluginApi::CreateSession(width, height);
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return nullptr;
            }
        }

        void xp_destroy_session(xp_session* session) {
            mobileclock::preview::PreviewPluginApi::DestroySession(session);
        }

        int xp_session_load_page(xp_session* session, const char* page) {
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

        int xp_session_current_page(xp_session* session, char* page, int capacity) {
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

        int xp_session_is_transitioning(xp_session* session) {
            if (session == nullptr) {
                return 0;
            }
            return session->value.Session().IsTransitioning() ? 1 : 0;
        }

        int xp_session_navigate_preview_route(xp_session* session, const char* target) {
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

        int xp_session_navigate_preview_route_path(xp_session* session, const char* path) {
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

        int xp_session_preview_route_graph(xp_session* session, char* graph, int capacity) {
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

        int xp_session_preview_page_title(xp_session* session, const char* page, char* title, int capacity) {
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

        int xp_session_apply_preview_scenario(xp_session* session, const char* page, const char* json) {
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

        int xp_session_export_preview_state(xp_session* session) {
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

        int xp_session_can_save_preview_state(xp_session* session) {
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

        int xp_session_reload_markup(xp_session* session, const char* page, const char* markup, const char* sourcePath) {
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

        int xp_session_resize(xp_session* session, int width, int height) {
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

        int xp_session_set_animation_playback_rate(xp_session* session, float value) {
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

        int xp_session_set_status(xp_session* session, const char* value) {
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

        int xp_session_pointer_down(xp_session* session, float x, float y) {
            if (session == nullptr) {
                return 0;
            }
            session->value.Session().PointerDown(x, y);
            return 1;
        }

        int xp_session_pointer_move(xp_session* session, float x, float y) {
            if (session == nullptr) {
                return 0;
            }
            session->value.Session().PointerMove(x, y);
            return 1;
        }

        int xp_session_pointer_up(xp_session* session, float x, float y) {
            if (session == nullptr) {
                return 0;
            }
            session->value.Session().PointerUp(x, y);
            return 1;
        }

        int xp_session_pointer_cancel(xp_session* session) {
            if (session == nullptr) {
                return 0;
            }
            session->value.Session().CancelPointer();
            return 1;
        }

        int xp_session_cursor_kind(xp_session* session, float x, float y) {
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

        int xp_session_inspect(
            xp_session* session,
            float x,
            float y,
            xp_session_inspection_result* result) {
            if (session == nullptr || result == nullptr) {
                return 0;
            }
            return mobileclock::preview::PreviewSessionApi(*session).Inspect(x, y, *result) ? 1 : 0;
        }

        int xp_session_set_inspection_wireframe(
            xp_session* session,
            float thickness,
            int lineStyle,
            xp_color color,
            xp_color marginColor,
            xp_color paddingColor) {
            if (session == nullptr) {
                return 0;
            }
            return mobileclock::preview::PreviewSessionApi(*session).SetInspectionWireframe(
                thickness, lineStyle, color, marginColor, paddingColor) ? 1 : 0;
        }

        int xp_session_set_selected_wireframe(
            xp_session* session,
            float thickness,
            int lineStyle,
            xp_color color,
            xp_color marginColor,
            xp_color paddingColor) {
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

        int xp_session_clear_inspection_wireframe(xp_session* session) {
            if (session == nullptr) {
                return 0;
            }
            mobileclock::preview::_details::ClearInspectionWireframe(*session);
            return 1;
        }

        int xp_session_clear_selected_inspection_element(xp_session* session) {
            if (session == nullptr) {
                return 0;
            }
            mobileclock::preview::_details::ClearSelectedWireframe(*session);
            return 1;
        }

        int xp_session_select_inspection_element(xp_session* session, const char* sourcePath, int line, int column) {
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

        int xp_session_pin_inspection_element(xp_session* session) {
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

        int xp_session_update(xp_session* session) {
            if (session == nullptr) {
                return 0;
            }
            return mobileclock::preview::PreviewSessionApi(*session).Update() ? 1 : 0;
        }

        int xp_session_render_angle_surface(
            xp_session* session,
            xp_angle_surface* surface,
            unsigned char* destination,
            int destinationStride,
            int destinationCapacity) {
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

        void xp_configure_logging(const char* filePath) {
            utility_helpers::logging::Configure({
                filePath == nullptr ? std::filesystem::path{} : std::filesystem::path(filePath),
            });
            utility_helpers::logging::Initialize("AndroidAppPreviewer");
            LOG_INFO("MobileClock.PreviewPlugin", "Logging initialized");
        }

        void xp_log_info(const char* message) {
            if (message != nullptr) {
                LOG_INFO("AndroidAppPreviewer.Interaction", "{}", message);
            }
        }

        xp_element* xp_create_element(const char* type) {
            try {
                xaml::bridge::lastError.clear();
                return reinterpret_cast<xp_element*>(new xaml::Element(xaml::ParseElementType(type)));
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return nullptr;
            }
        }

        void xp_destroy_element(xp_element* element) {
            delete reinterpret_cast<xaml::Element*>(element);
        }

        int xp_items_remove_item(xp_element* target) {
            try {
                xaml::bridge::lastError.clear();
                if (target == nullptr) {
                    throw std::invalid_argument("target is required");
                }
                return xaml::bridge::_details::RemoveItem(*reinterpret_cast<xaml::Element*>(target)) ? 1 : 0;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return -1;
            }
        }

        int xp_add_child(xp_element* parent, xp_element* child) {
            try {
                xaml::bridge::lastError.clear();
                if (parent == nullptr || child == nullptr) {
                    throw std::invalid_argument("parent and child are required");
                }
                xaml::ValidateChild(
                    *reinterpret_cast<const xaml::Element*>(parent),
                    *reinterpret_cast<const xaml::Element*>(child));
                reinterpret_cast<xaml::Element*>(parent)->AddChild(
                    std::unique_ptr<xaml::Element>(reinterpret_cast<xaml::Element*>(child)));
                return 1;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        int xp_set_attribute(
            xp_element* element,
            const char* name,
            const char* value) {
            try {
                xaml::bridge::lastError.clear();
                if (element == nullptr) {
                    throw std::invalid_argument("element is required");
                }
                xaml::SetAttribute(*reinterpret_cast<xaml::Element*>(element), name, value);
                return 1;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        xp_element* xp_find_element(xp_element* root, const char* id) {
            try {
                xaml::bridge::lastError.clear();
                if (root == nullptr || id == nullptr || *id == '\0') {
                    throw std::invalid_argument("root and id are required");
                }
                return reinterpret_cast<xp_element*>(xaml::bridge::_details::FindElement(
                    *reinterpret_cast<xaml::Element*>(root), id));
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return nullptr;
            }
        }

        int xp_find_element_count(xp_element* root, const char* id) {
            try {
                xaml::bridge::lastError.clear();
                if (root == nullptr || id == nullptr || *id == '\0') {
                    throw std::invalid_argument("root and id are required");
                }
                return xaml::bridge::_details::CountElements(*reinterpret_cast<xaml::Element*>(root), id);
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return -1;
            }
        }

        xp_element* xp_find_element_at(xp_element* root, const char* id, int index) {
            try {
                xaml::bridge::lastError.clear();
                if (root == nullptr || id == nullptr || *id == '\0' || index < 0) {
                    throw std::invalid_argument("root, id and non-negative index are required");
                }
                return reinterpret_cast<xp_element*>(xaml::bridge::_details::FindElementAt(
                    *reinterpret_cast<xaml::Element*>(root), id, index));
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return nullptr;
            }
        }

        int xp_add_storyboard_animation(xp_element* element, int trigger, const char* name,
            const char* const* keys, const char* const* values, int count) {
            try {
                xaml::bridge::lastError.clear();
                if (element == nullptr || trigger < 0 || trigger > 6 || count < 0
                    || (count > 0 && (keys == nullptr || values == nullptr))) {
                    throw std::invalid_argument("invalid storyboard animation");
                }
                xaml::Storyboard storyboard;
                storyboard.trigger = static_cast<xaml::AnimationTrigger>(trigger);
                if (name != nullptr) {
                    if (*name == '\\0') {
                        throw std::invalid_argument("animation name is required");
                    }
                    xaml::AnimationTrack track;
                    track.name = name;
                    for (int index = 0; index < count; ++index) {
                        if (keys[index] == nullptr || values[index] == nullptr) {
                            throw std::invalid_argument("animation settings are required");
                        }
                        track.settings.Set(keys[index], values[index]);
                    }
                    storyboard.tracks.push_back(std::move(track));
                }
                reinterpret_cast<xaml::Element*>(element)->AddStoryboard(std::move(storyboard));
                return 1;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        int xp_attach_animations(xp_element* root, xp_animation_controller* animations) {
            try {
                xaml::bridge::lastError.clear();
                if (root == nullptr) {
                    throw std::invalid_argument("root is required");
                }
                if (animations == nullptr) {
                    throw std::invalid_argument("animations is required");
                }
                animations->value.Attach(*reinterpret_cast<xaml::Element*>(root));
                return 1;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        int xp_set_page_transition(
            xp_element* root,
            xp_animation_controller* animations,
            const char* from,
            const char* to,
            int backward,
            int visible) {
            try {
                xaml::bridge::lastError.clear();
                if (root == nullptr || animations == nullptr || from == nullptr || to == nullptr) {
                    throw std::invalid_argument("root, animations, from and to are required");
                }
                animations->value.SetPageTransition(
                    *reinterpret_cast<xaml::Element*>(root),
                    from,
                    to,
                    backward != 0,
                    visible != 0);
                return 1;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        int xp_add_storyboard_track(
            xp_element* element,
            int trigger,
            int property,
            float from,
            float to,
            int durationMilliseconds,
            int easing) {
            try {
                xaml::bridge::lastError.clear();
                if (element == nullptr || trigger < 0 || trigger > 6 || property < 0 || property > 5
                    || durationMilliseconds < 0 || easing < 0 || easing > 1) {
                    throw std::invalid_argument("invalid storyboard track");
                }
                xaml::Storyboard storyboard;
                storyboard.trigger = static_cast<xaml::AnimationTrigger>(trigger);
                storyboard.tracks.push_back({
                    static_cast<xaml::AnimatedProperty>(property),
                    from,
                    to,
                    std::isnan(from),
                    std::isnan(to),
                    std::chrono::milliseconds(durationMilliseconds),
                    static_cast<xaml::Easing>(easing),
                });
                reinterpret_cast<xaml::Element*>(element)->AddStoryboard(std::move(storyboard));
                return 1;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        int xp_add_visual_state_track(
            xp_element* scope,
            const char* groupName,
            const char* stateName,
            const char* targetName,
            int property,
            float from,
            float to,
            int durationMilliseconds,
            int easing) {
            try {
                xaml::bridge::lastError.clear();
                if (scope == nullptr || groupName == nullptr || stateName == nullptr || targetName == nullptr
                    || *groupName == '\0' || *stateName == '\0' || *targetName == '\0'
                    || property < 0 || property > 5 || durationMilliseconds < 0 || easing < 0 || easing > 1) {
                    throw std::invalid_argument("invalid visual state track");
                }
                auto& groups = reinterpret_cast<xaml::Element*>(scope)->VisualStateGroups();
                auto group = std::find_if(groups.begin(), groups.end(), [groupName](const xaml::VisualStateGroup& value) {
                    return value.name == groupName;
                });
                if (group == groups.end()) {
                    groups.push_back({groupName});
                    group = std::prev(groups.end());
                }
                auto state = std::find_if(group->states.begin(), group->states.end(), [stateName](const xaml::VisualState& value) {
                    return value.name == stateName;
                });
                if (state == group->states.end()) {
                    group->states.push_back({stateName});
                    state = std::prev(group->states.end());
                }
                state->tracks.push_back({targetName, {
                    static_cast<xaml::AnimatedProperty>(property), from, to, std::isnan(from), std::isnan(to),
                    std::chrono::milliseconds(durationMilliseconds), static_cast<xaml::Easing>(easing)}});
                return 1;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        int xp_go_to_visual_state(
            xp_element* scope,
            const char* groupName,
            const char* stateName,
            int useTransitions) {
            try {
                xaml::bridge::lastError.clear();
                if (scope == nullptr || groupName == nullptr || stateName == nullptr) {
                    throw std::invalid_argument("scope, groupName and stateName are required");
                }
                return xaml::VisualStateManager::GoToState(*reinterpret_cast<xaml::Element*>(scope),
                    groupName, stateName, useTransitions != 0) ? 1 : 0;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        int xp_supported_attribute_count(const char* elementType) {
            try {
                xaml::bridge::lastError.clear();
                if (elementType == nullptr) {
                    throw std::invalid_argument("element type is required");
                }
                if (std::string_view(elementType) == "columnDefinition" || std::string_view(elementType) == "rowDefinition") {
                    return 1;
                }
                return static_cast<int>(xaml::SupportedAttributeNames(xaml::ParseElementType(elementType)).size());
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        const char* xp_supported_attribute_name(
            const char* elementType,
            int index) {
            try {
                xaml::bridge::lastError.clear();
                if (elementType == nullptr || index < 0) {
                    throw std::invalid_argument("element type and non-negative index are required");
                }
                if (std::string_view(elementType) == "columnDefinition" && index == 0) {
                    return "width";
                }
                if (std::string_view(elementType) == "rowDefinition" && index == 0) {
                    return "height";
                }
                const std::vector<std::string_view> names = xaml::SupportedAttributeNames(
                    xaml::ParseElementType(elementType));
                if (index >= static_cast<int>(names.size())) {
                    throw std::out_of_range("attribute index is out of range");
                }
                return names[static_cast<size_t>(index)].data();
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return "";
            }
        }

        int xp_supported_element_count(void) {
            return 18;
        }

        const char* xp_supported_element_name(int index) {
            static constexpr std::string_view names[]{
                "Page", "StackPanel", "Grid", "Border", "TextBlock", "Button", "IconButton", "ToggleSwitch",
                "ScrollViewer", "Image", "SvgImage", "ListView", "ListView.ItemTemplate", "DataTemplate",
                "columnDefinitions", "columnDefinition", "rowDefinitions", "rowDefinition"
            };
            return index >= 0 && index < static_cast<int>(std::size(names)) ? names[index].data() : "";
        }

        int xp_layout(xp_element* root, float width, float height) {
            try {
                xaml::bridge::lastError.clear();
                if (root == nullptr) {
                    throw std::invalid_argument("root is required");
                }
                xaml::layout(*reinterpret_cast<xaml::Element*>(root), {width, height});
                return 1;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        xp_element* xp_hit_test(xp_element* root, float x, float y) {
            try {
                xaml::bridge::lastError.clear();
                if (root == nullptr) {
                    throw std::invalid_argument("root is required");
                }
                return reinterpret_cast<xp_element*>(xaml::HitTest(
                    *reinterpret_cast<xaml::Element*>(root), x, y));
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return nullptr;
            }
        }

        // Возвращает previewer-у любой видимый элемент под указателем,
        // включая элементы, которые не являются enabled или interactive.
        xp_element* xp_hit_test_visual(xp_element* root, float x, float y) {
            try {
                xaml::bridge::lastError.clear();
                if (root == nullptr) {
                    throw std::invalid_argument("root is required");
                }
                return reinterpret_cast<xp_element*>(xaml::bridge::_details::HitTestVisual(
                    *reinterpret_cast<xaml::Element*>(root), x, y));
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return nullptr;
            }
        }

        int xp_hit_test_cursor_kind(xp_element* root, float x, float y) {
            try {
                xaml::bridge::lastError.clear();
                if (root == nullptr) {
                    throw std::invalid_argument("root is required");
                }
                auto& nativeRoot = *reinterpret_cast<xaml::Element*>(root);
                xaml::Element* const visual = xaml::bridge::_details::HitTestVisual(nativeRoot, x, y);
                if (visual == nullptr) {
                    return 0;
                }
                xaml::Element* const interactive = xaml::HitTest(nativeRoot, x, y);
                if (interactive != nullptr && interactive->Type() != xaml::ElementType::scrollViewer) {
                    return 1;
                }
                for (xaml::Element* element = visual; element != nullptr; element = element->Parent()) {
                    if (element->Type() == xaml::ElementType::scrollViewer) {
                        return 2;
                    }
                }
                return 0;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        // Копирует рассчитанные layout-границы элемента в структуру C bridge.
        int xp_element_bounds(const xp_element* element, xp_rect* bounds) {
            try {
                xaml::bridge::lastError.clear();
                if (element == nullptr || bounds == nullptr) {
                    throw std::invalid_argument("element and bounds are required");
                }
                const xaml::Rect elementBounds = reinterpret_cast<const xaml::Element*>(element)->Bounds();
                *bounds = {elementBounds.x, elementBounds.y, elementBounds.width, elementBounds.height};
                return 1;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        int xp_get_scroll_offsets(
            xp_element* root,
            const char* scrollViewerId,
            float* horizontalOffset,
            float* verticalOffset) {
            try {
                xaml::bridge::lastError.clear();
                if (root == nullptr || scrollViewerId == nullptr || *scrollViewerId == '\0'
                    || horizontalOffset == nullptr || verticalOffset == nullptr) {
                    throw std::invalid_argument("root, scroll viewer id and offsets are required");
                }
                xaml::Element* const scrollViewer = xaml::bridge::_details::FindElement(
                    *reinterpret_cast<xaml::Element*>(root), scrollViewerId);
                if (scrollViewer == nullptr || scrollViewer->Type() != xaml::ElementType::scrollViewer) {
                    return 0;
                }
                *horizontalOffset = scrollViewer->HorizontalOffset();
                *verticalOffset = scrollViewer->VerticalOffset();
                return 1;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        int xp_set_scroll_offsets(
            xp_element* root,
            const char* scrollViewerId,
            float horizontalOffset,
            float verticalOffset) {
            try {
                xaml::bridge::lastError.clear();
                if (root == nullptr || scrollViewerId == nullptr || *scrollViewerId == '\0'
                    || !std::isfinite(horizontalOffset) || !std::isfinite(verticalOffset)) {
                    throw std::invalid_argument("root, scroll viewer id and finite offsets are required");
                }
                xaml::Element* const scrollViewer = xaml::bridge::_details::FindElement(
                    *reinterpret_cast<xaml::Element*>(root), scrollViewerId);
                if (scrollViewer == nullptr || scrollViewer->Type() != xaml::ElementType::scrollViewer) {
                    return 0;
                }
                scrollViewer->SetHorizontalOffset(horizontalOffset);
                scrollViewer->SetVerticalOffset(verticalOffset);
                return 1;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        int xp_scroll_by(xp_element* root, float x, float y, float horizontalDelta, float verticalDelta) {
            try {
                xaml::bridge::lastError.clear();
                if (root == nullptr || !std::isfinite(horizontalDelta) || !std::isfinite(verticalDelta)) {
                    throw std::invalid_argument("root and finite deltas are required");
                }
                xaml::Element* element = xaml::bridge::_details::FindScrollViewer(
                    *reinterpret_cast<xaml::Element*>(root), x, y);
                if (element == nullptr) {
                    return 0;
                }
                const float horizontalOffset = std::clamp(
                    element->HorizontalOffset() + horizontalDelta,
                    0.0f,
                    std::max(0.0f, element->Extent().width - element->Viewport().width));
                const float verticalOffset = std::clamp(
                    element->VerticalOffset() + verticalDelta,
                    0.0f,
                    std::max(0.0f, element->Extent().height - element->Viewport().height));
                const bool changed = horizontalOffset != element->HorizontalOffset()
                    || verticalOffset != element->VerticalOffset();
                if (!changed) {
                    return 0;
                }
                element->SetHorizontalOffset(horizontalOffset);
                element->SetVerticalOffset(verticalOffset);
                return 1;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        int xp_scroll_begin(
            xp_element* root,
            xp_animation_controller* animations,
            float x,
            float y) {
            try {
                xaml::bridge::lastError.clear();
                if (root == nullptr || animations == nullptr || !std::isfinite(x) || !std::isfinite(y)) {
                    throw std::invalid_argument("root, animations and finite coordinates are required");
                }
                xaml::Element* const scrollViewer = xaml::bridge::_details::FindScrollViewer(
                    *reinterpret_cast<xaml::Element*>(root), x, y);
                if (scrollViewer == nullptr) {
                    return 0;
                }
                animations->scrollController.Begin(*scrollViewer);
                return 1;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        int xp_scroll_drag(xp_animation_controller* animations, float verticalDelta) {
            try {
                xaml::bridge::lastError.clear();
                if (animations == nullptr || !std::isfinite(verticalDelta)) {
                    throw std::invalid_argument("animations and finite vertical delta are required");
                }
                return animations->scrollController.Drag(verticalDelta) ? 1 : 0;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        void xp_scroll_end(xp_animation_controller* animations) {
            if (animations != nullptr) {
                animations->scrollController.End();
            }
        }

        int xp_set_render_offset_x(xp_element* element, float value) {
            try {
                xaml::bridge::lastError.clear();
                if (element == nullptr || !std::isfinite(value)) {
                    throw std::invalid_argument("element and finite value are required");
                }
                reinterpret_cast<xaml::Element*>(element)->SetRenderOffsetX(value);
                return 1;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        int xp_animate_render_offset_x(
            xp_element* element,
            xp_animation_controller* animations,
            float value,
            int durationMilliseconds) {
            try {
                xaml::bridge::lastError.clear();
                if (element == nullptr || animations == nullptr || !std::isfinite(value)
                    || durationMilliseconds < 0) {
                    throw std::invalid_argument("element, animations, value and duration are required");
                }
                xaml::Element& target = *reinterpret_cast<xaml::Element*>(element);
                animations->value.Animations().Animate(
                    target,
                    xaml::AnimatedProperty::renderOffsetX,
                    target.RenderOffsetX(),
                    value,
                    std::chrono::milliseconds(durationMilliseconds));
                return 1;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        const char* xp_element_id(const xp_element* element) {
            if (element == nullptr) {
                return "";
            }
            return reinterpret_cast<const xaml::Element*>(element)->Id().c_str();
        }

        int xp_handle_tap(xp_element* element, xp_animation_controller* animations) {
            try {
                xaml::bridge::lastError.clear();
                if (element == nullptr || animations == nullptr) {
                    throw std::invalid_argument("element and animations are required");
                }
                xaml::Element& target = *reinterpret_cast<xaml::Element*>(element);
                if (!xaml::HandleTap(target)) {
                    return 0;
                }
                if (target.Type() == xaml::ElementType::toggleSwitch) {
                    animations->value.Animations().Start(target, xaml::AnimationTrigger::toggled);
                }
                return 1;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        int xp_handle_pointer_down(xp_element* element, xp_animation_controller* animations) {
            try {
                xaml::bridge::lastError.clear();
                if (element == nullptr || animations == nullptr) {
                    throw std::invalid_argument("element and animations are required");
                }
                xaml::Element& target = *reinterpret_cast<xaml::Element*>(element);
                animations->value.Animations().Start(target, xaml::AnimationTrigger::pointerDown);
                return 1;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        int xp_handle_pointer_up(xp_element* element, xp_animation_controller* animations) {
            try {
                xaml::bridge::lastError.clear();
                if (element == nullptr || animations == nullptr) {
                    throw std::invalid_argument("element and animations are required");
                }
                xaml::Element& target = *reinterpret_cast<xaml::Element*>(element);
                if (target.Type() == xaml::ElementType::button) {
                    animations->value.Animations().Start(target, xaml::AnimationTrigger::pointerUp);
                    return 1;
                }
                return AndroidAppPreviewerPluginSDK::xp_handle_tap(element, animations);
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        xp_animation_controller* xp_create_animation_controller(void) {
            try {
                xaml::bridge::lastError.clear();
                return new xp_animation_controller();
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return nullptr;
            }
        }

        void xp_destroy_animation_controller(xp_animation_controller* animations) {
            delete animations;
        }

        xp_interaction_controller* xp_create_interaction_controller(void) {
            return new xp_interaction_controller();
        }

        void xp_destroy_interaction_controller(xp_interaction_controller* controller) {
            delete controller;
        }

        int xp_interaction_pointer_down(
            xp_interaction_controller* controller,
            xp_element* root,
            xp_animation_controller* animations,
            float x,
            float y) {
            if (controller == nullptr || root == nullptr || animations == nullptr) {
                return 0;
            }
            controller->value.PointerDown(
                *reinterpret_cast<xaml::Element*>(root),
                animations->value.Animations(),
                x,
                y);
            return controller->value.HasCapture() ? 1 : 0;
        }

        int xp_interaction_pointer_move(xp_interaction_controller* controller, float x, float y) {
            return controller != nullptr && controller->value.PointerMove(x, y) ? 1 : 0;
        }

        int xp_interaction_pointer_up(
            xp_interaction_controller* controller,
            xp_element* root,
            xp_animation_controller* animations,
            float x,
            float y,
            xp_interaction_result* result) {
            if (controller == nullptr || root == nullptr || animations == nullptr || result == nullptr) {
                return 0;
            }
            const xaml::GestureResult nativeResult = controller->value.PointerUp(
                *reinterpret_cast<xaml::Element*>(root), animations->value.Animations(), x, y);
            result->kind = static_cast<int>(nativeResult.kind);
            result->direction = static_cast<int>(nativeResult.direction);
            result->target = reinterpret_cast<xp_element*>(nativeResult.target);
            result->item_index = nativeResult.itemIndex;
            return 1;
        }

        int xp_interaction_scroll_wheel(
            xp_interaction_controller* controller,
            xp_element* root,
            float x,
            float y,
            float horizontalDelta,
            float verticalDelta) {
            return controller != nullptr && root != nullptr
                && controller->value.ScrollWheel(*reinterpret_cast<xaml::Element*>(root), x, y, horizontalDelta, verticalDelta) ? 1 : 0;
        }

        int xp_interaction_update(xp_interaction_controller* controller) {
            return controller != nullptr && controller->value.Update() ? 1 : 0;
        }

        int xp_set_animation_playback_rate(
            xp_animation_controller* animations,
            float playbackRate) {
            try {
                xaml::bridge::lastError.clear();
                if (animations == nullptr || !std::isfinite(playbackRate)) {
                    throw std::invalid_argument("animations and finite playbackRate are required");
                }
                animations->value.SetPlaybackRate(playbackRate);
                return 1;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        int xp_update_animations(xp_animation_controller* animations) {
            try {
                xaml::bridge::lastError.clear();
                if (animations == nullptr) {
                    throw std::invalid_argument("animations are required");
                }
                const bool wasScrolling = animations->scrollController.Update();
                return animations->value.Update() || wasScrolling ? 1 : 0;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return -1;
            }
        }

        xp_angle_surface* xp_create_angle_surface(
            int width,
            int height,
            const char* fontPath,
            const char* resourceRoot) {
            try {
                xaml::bridge::lastError.clear();
                if (fontPath == nullptr || resourceRoot == nullptr) {
                    throw std::invalid_argument("fontPath and resourceRoot are required");
                }
                return new xp_angle_surface(width, height, fontPath, resourceRoot);
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return nullptr;
            }
        }

        void xp_destroy_angle_surface(xp_angle_surface* surface) {
            delete surface;
        }

        int xp_render_angle_surface(
            xp_angle_surface* surface,
            const xp_element* root,
            unsigned char* destination,
            int destinationStride,
            int destinationCapacity) {
            try {
                xaml::bridge::lastError.clear();
                if (surface == nullptr || root == nullptr || destination == nullptr
                    || destinationStride < surface->width * 4
                    || destinationCapacity / destinationStride < surface->height) {
                    throw std::invalid_argument("Invalid persistent ANGLE render arguments");
                }
                surface->value.Render(
                    *reinterpret_cast<xaml::Element*>(const_cast<xp_element*>(root)),
                    destination,
                    destinationStride);
                return 1;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }

        int xp_render(
            const xp_element* root,
            xp_command* destination,
            int capacity) {
            try {
                xaml::bridge::lastError.clear();
                if (root == nullptr) {
                    throw std::invalid_argument("root is required");
                }
                mobileclock::preview::rendering::RecordingBackend backend;
                xaml::Render(*reinterpret_cast<xaml::Element*>(const_cast<xp_element*>(root)), backend);
                const auto& commands = backend.Commands();
                if (destination != nullptr && capacity > 0) {
                    const size_t count = std::min(commands.size(), static_cast<size_t>(capacity));
                    std::copy_n(commands.data(), count, destination);
                }
                return static_cast<int>(commands.size());
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return -1;
            }
        }

        int xp_render_angle(
            const xp_element* root,
            const char* fontPath,
            int width,
            int height,
            const char* resourceRoot,
            unsigned char* destination,
            int destinationStride,
            int destinationCapacity) {
            try {
                xaml::bridge::lastError.clear();
                if (root == nullptr || fontPath == nullptr || destination == nullptr
                    || width <= 0 || height <= 0 || destinationStride < width * 4
                    || destinationCapacity / destinationStride < height) {
                    throw std::invalid_argument("Invalid ANGLE render arguments");
                }

                mobileclock::preview::rendering::AngleRenderSurface surface(width, height, fontPath, resourceRoot);
                surface.Render(
                    *reinterpret_cast<xaml::Element*>(const_cast<xp_element*>(root)),
                    destination,
                    destinationStride);
                return 1;
            } catch (const std::exception& error) {
                xaml::bridge::lastError = error.what();
                return 0;
            }
        }
    } // extern C
} // namespace AndroidAppPreviewerPluginSDK