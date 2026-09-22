#pragma once

#include <AndroidAppPreviewer.PluginSDK/AndroidAppPreviewerPlugin.h>

#ifdef __cplusplus
namespace AndroidAppPreviewerPluginSDK {
#endif
    //
    // Внутренние статические API-группы preview-plugin. Этот заголовок не входит в SDK-пакет.
    //
    class MetadataApi final {
    public:
        static uint32_t xp_get_abi_version(void);
        static int xp_get_plugin_info(
            char* pluginInfoJson,
            int capacity
        );
        static int xp_get_initial_page_id(
            void* session,
            char* pageId,
            int capacity
        );
        static int xp_get_navigation_graph(
            void* session,
            char* graphJson,
            int capacity
        );
        static int xp_navigate(
            void* session,
            const char* navigationRequestJson
        );
        static const char* xp_last_error(void);
    };

    class SessionApi final {
    public:
        static xp_session* xp_create_session(
            int width,
            int height
        );
        static void xp_destroy_session(xp_session* session);
        static int xp_session_load_page(
            xp_session* session,
            const char* page
        );
        static int xp_session_current_page(
            xp_session* session,
            char* page,
            int capacity
        );
        static int xp_session_is_transitioning(xp_session* session);
        static int xp_session_navigate_preview_route(
            xp_session* session,
            const char* target
        );
        static int xp_session_navigate_preview_route_path(
            xp_session* session,
            const char* path
        );
        static int xp_session_preview_route_graph(
            xp_session* session,
            char* graph,
            int capacity
        );
        static int xp_session_preview_page_title(
            xp_session* session,
            const char* page,
            char* title,
            int capacity
        );
        static int xp_session_apply_preview_scenario(
            xp_session* session,
            const char* page,
            const char* json
        );
        static int xp_session_export_preview_state(xp_session* session);
        static int xp_session_can_save_preview_state(xp_session* session);
        static int xp_session_reload_markup(
            xp_session* session,
            const char* page,
            const char* markup,
            const char* sourcePath
        );
        static int xp_session_resize(
            xp_session* session,
            int width,
            int height
        );
        static int xp_session_set_animation_playback_rate(
            xp_session* session,
            float value
        );
        static int xp_session_set_status(
            xp_session* session,
            const char* value
        );
        static int xp_session_pointer_down(
            xp_session* session,
            float x,
            float y
        );
        static int xp_session_pointer_move(
            xp_session* session,
            float x,
            float y
        );
        static int xp_session_pointer_up(
            xp_session* session,
            float x,
            float y
        );
        static int xp_session_pointer_cancel(xp_session* session);
        static int xp_session_cursor_kind(
            xp_session* session,
            float x,
            float y
        );
        static int xp_session_inspect(
            xp_session* session,
            float x,
            float y,
            xp_session_inspection_result* result
        );
        static int xp_session_set_inspection_wireframe(
            xp_session* session,
            float thickness,
            int lineStyle,
            xp_color color,
            xp_color marginColor,
            xp_color paddingColor
        );
        static int xp_session_set_selected_wireframe(
            xp_session* session,
            float thickness,
            int lineStyle,
            xp_color color,
            xp_color marginColor,
            xp_color paddingColor
        );
        static int xp_session_clear_inspection_wireframe(xp_session* session);
        static int xp_session_clear_selected_inspection_element(xp_session* session);
        static int xp_session_select_inspection_element(
            xp_session* session,
            const char* sourcePath,
            int line,
            int column
        );
        static int xp_session_pin_inspection_element(xp_session* session);
        static int xp_session_update(xp_session* session);
        static int xp_session_render_angle_surface(
            xp_session* session,
            xp_angle_surface* surface,
            unsigned char* destination,
            int destinationStride,
            int destinationCapacity
        );
    };

    class ElementApi final {
    public:
        static xp_element* xp_create_element(const char* type);
        static void xp_destroy_element(xp_element* element);
        static int xp_items_remove_item(xp_element* target);
        static int xp_add_child(
            xp_element* parent,
            xp_element* child
        );
        static int xp_set_attribute(
            xp_element* element,
            const char* name,
            const char* value
        );
        static xp_element* xp_find_element(
            xp_element* root,
            const char* id
        );
        static int xp_find_element_count(
            xp_element* root,
            const char* id
        );
        static xp_element* xp_find_element_at(
            xp_element* root,
            const char* id,
            int index
        );
        static int xp_layout(
            xp_element* root,
            float width,
            float height
        );
        static xp_element* xp_hit_test(
            xp_element* root,
            float x,
            float y
        );
        static xp_element* xp_hit_test_visual(
            xp_element* root,
            float x,
            float y
        );
        static int xp_hit_test_cursor_kind(
            xp_element* root,
            float x,
            float y
        );
        static int xp_element_bounds(
            const xp_element* element,
            xp_rect* bounds
        );
        static const char* xp_element_id(const xp_element* element);
        static int xp_get_scroll_offsets(
            xp_element* root,
            const char* scroll_viewer_id,
            float* horizontal_offset,
            float* vertical_offset
        );
        static int xp_set_scroll_offsets(
            xp_element* root,
            const char* scroll_viewer_id,
            float horizontal_offset,
            float vertical_offset
        );
        static int xp_scroll_by(
            xp_element* root,
            float x,
            float y,
            float horizontalDelta,
            float verticalDelta
        );
    };

    class XamlCompletionApi final {
    public:
        static int xp_supported_attribute_count(const char* elementType);
        static const char* xp_supported_attribute_name(
            const char* elementType,
            int index
        );
        static int xp_supported_element_count(void);
        static const char* xp_supported_element_name(int index);
    };

    class InteractionApi final {
    public:
        static int xp_add_storyboard_animation(
            xp_element* element,
            int trigger,
            const char* name,
            const char* const* keys,
            const char* const* values,
            int count
        );
        static int xp_attach_animations(
            xp_element* root,
            xp_animation_controller* animations
        );
        static int xp_set_page_transition(
            xp_element* root,
            xp_animation_controller* animations,
            const char* from,
            const char* to,
            int backward,
            int visible
        );
        static int xp_add_storyboard_track(
            xp_element* element,
            int trigger,
            int property,
            float from,
            float to,
            int durationMilliseconds,
            int easing
        );
        static int xp_add_visual_state_track(
            xp_element* scope,
            const char* groupName,
            const char* stateName,
            const char* targetName,
            int property,
            float from,
            float to,
            int durationMilliseconds,
            int easing
        );
        static int xp_go_to_visual_state(
            xp_element* scope,
            const char* groupName,
            const char* stateName,
            int useTransitions
        );
        static int xp_scroll_begin(
            xp_element* root,
            xp_animation_controller* animations,
            float x,
            float y
        );
        static int xp_scroll_drag(
            xp_animation_controller* animations,
            float verticalDelta
        );
        static void xp_scroll_end(xp_animation_controller* animations);
        static int xp_set_render_offset_x(
            xp_element* element,
            float value
        );
        static int xp_animate_render_offset_x(
            xp_element* element,
            xp_animation_controller* animations,
            float value,
            int duration_milliseconds
        );
        static int xp_handle_tap(
            xp_element* element,
            xp_animation_controller* animations
        );
        static int xp_handle_pointer_down(
            xp_element* element,
            xp_animation_controller* animations
        );
        static int xp_handle_pointer_up(
            xp_element* element,
            xp_animation_controller* animations
        );
        static xp_animation_controller* xp_create_animation_controller(void);
        static void xp_destroy_animation_controller(xp_animation_controller* animations);
        static xp_interaction_controller* xp_create_interaction_controller(void);
        static void xp_destroy_interaction_controller(xp_interaction_controller* controller);
        static int xp_interaction_pointer_down(
            xp_interaction_controller* controller,
            xp_element* root,
            xp_animation_controller* animations,
            float x,
            float y
        );
        static int xp_interaction_pointer_move(
            xp_interaction_controller* controller,
            float x,
            float y
        );
        static int xp_interaction_pointer_up(
            xp_interaction_controller* controller,
            xp_element* root,
            xp_animation_controller* animations,
            float x,
            float y,
            xp_interaction_result* result
        );
        static int xp_interaction_scroll_wheel(
            xp_interaction_controller* controller,
            xp_element* root,
            float x,
            float y,
            float horizontal_delta,
            float vertical_delta
        );
        static int xp_interaction_update(xp_interaction_controller* controller);
        static int xp_set_animation_playback_rate(
            xp_animation_controller* animations,
            float playbackRate
        );
        static int xp_update_animations(xp_animation_controller* animations);
    };

    class RenderingApi final {
    public:
        static xp_angle_surface* xp_create_angle_surface(
            int width,
            int height,
            const char* fontPath,
            const char* resourceRoot
        );
        static void xp_destroy_angle_surface(xp_angle_surface* surface);
        static int xp_render_angle_surface(
            xp_angle_surface* surface,
            const xp_element* root,
            unsigned char* destination,
            int destinationStride,
            int destinationCapacity
        );
        static int xp_render(
            const xp_element* root,
            xp_command* destination,
            int capacity
        );
        static int xp_render_angle(
            const xp_element* root,
            const char* fontPath,
            int width,
            int height,
            const char* resourceRoot,
            unsigned char* destination,
            int destinationStride,
            int destinationCapacity
        );
    };

    class LoggingApi final {
    public:
        static void xp_configure_logging(const char* filePath);
        static void xp_log_info(const char* message);
    };
#ifdef __cplusplus
} // namespace AndroidAppPreviewerPluginSDK
#endif