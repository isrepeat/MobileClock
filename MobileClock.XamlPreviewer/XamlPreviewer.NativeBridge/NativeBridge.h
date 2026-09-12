#pragma once
#ifdef _WIN32
#define XAML_RUNTIME_BRIDGE_API __declspec(dllexport)
#else
#define XAML_RUNTIME_BRIDGE_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xr_element xr_element;
typedef struct xr_animation_controller xr_animation_controller;
typedef struct xr_interaction_controller xr_interaction_controller;
typedef struct xr_angle_surface xr_angle_surface;
typedef struct mc_session mc_session;

typedef struct xr_rect {
    float x;
    float y;
    float width;
    float height;
} xr_rect;

typedef struct mc_inspection_result {
    int line;
    int column;
    char sourcePath[1024];
    xr_rect bounds;
} mc_inspection_result;

typedef struct xr_color {
    float red;
    float green;
    float blue;
    float alpha;
} xr_color;

typedef struct xr_interaction_result {
    int kind;
    int direction;
    xr_element* target;
    int item_index;
} xr_interaction_result;

typedef enum xr_command_type {
    xr_command_type_begin_clip,
    xr_command_type_end_clip,
    xr_command_type_outline,
    xr_command_type_rounded_rect,
    xr_command_type_rounded_rect_outline,
    xr_command_type_text,
    xr_command_type_image,
} xr_command_type;

typedef struct xr_command {
    int type;
    xr_rect bounds;
    xr_color color;
    float value;
    char text[512];
    char auxiliary[128];
} xr_command;

XAML_RUNTIME_BRIDGE_API const char* xr_last_error(void);
XAML_RUNTIME_BRIDGE_API mc_session* mc_create_session(int width, int height);
XAML_RUNTIME_BRIDGE_API void mc_destroy_session(mc_session* session);
XAML_RUNTIME_BRIDGE_API int mc_load_page(mc_session* session, const char* page);
XAML_RUNTIME_BRIDGE_API int mc_current_page(mc_session* session, char* page, int capacity);
XAML_RUNTIME_BRIDGE_API int mc_is_transitioning(mc_session* session);
XAML_RUNTIME_BRIDGE_API int mc_navigate_preview_route(mc_session* session, const char* target);
XAML_RUNTIME_BRIDGE_API int mc_navigate_preview_route_path(mc_session* session, const char* path);
XAML_RUNTIME_BRIDGE_API int mc_preview_route_graph(mc_session* session, char* graph, int capacity);
XAML_RUNTIME_BRIDGE_API int mc_preview_page_title(mc_session* session, const char* page, char* title, int capacity);
XAML_RUNTIME_BRIDGE_API int mc_apply_preview_scenario(
    mc_session* session,
    const char* page,
    const char* json);
XAML_RUNTIME_BRIDGE_API int mc_reload_markup(mc_session* session, const char* page, const char* markup, const char* sourcePath);
XAML_RUNTIME_BRIDGE_API int mc_resize(mc_session* session, int width, int height);
XAML_RUNTIME_BRIDGE_API int mc_set_animation_playback_rate(mc_session* session, float value);
XAML_RUNTIME_BRIDGE_API int mc_set_status(mc_session* session, const char* value);
XAML_RUNTIME_BRIDGE_API int mc_pointer_down(mc_session* session, float x, float y);
XAML_RUNTIME_BRIDGE_API int mc_pointer_move(mc_session* session, float x, float y);
XAML_RUNTIME_BRIDGE_API int mc_pointer_up(mc_session* session, float x, float y);
XAML_RUNTIME_BRIDGE_API int mc_pointer_cancel(mc_session* session);
XAML_RUNTIME_BRIDGE_API int mc_cursor_kind(mc_session* session, float x, float y);
XAML_RUNTIME_BRIDGE_API int mc_inspect(
    mc_session* session,
    float x,
    float y,
    mc_inspection_result* result);
XAML_RUNTIME_BRIDGE_API int mc_set_inspection_wireframe(
    mc_session* session,
    float thickness,
    int lineStyle,
    xr_color color,
    xr_color marginColor,
    xr_color paddingColor);
XAML_RUNTIME_BRIDGE_API int mc_set_selected_wireframe(
    mc_session* session,
    float thickness,
    int lineStyle,
    xr_color color,
    xr_color marginColor,
    xr_color paddingColor);
XAML_RUNTIME_BRIDGE_API int mc_clear_inspection_wireframe(mc_session* session);
XAML_RUNTIME_BRIDGE_API int mc_clear_selected_inspection_element(mc_session* session);
XAML_RUNTIME_BRIDGE_API int mc_select_inspection_element(
    mc_session* session,
    const char* sourcePath,
    int line,
    int column);
XAML_RUNTIME_BRIDGE_API int mc_pin_inspection_element(mc_session* session);
XAML_RUNTIME_BRIDGE_API int mc_update(mc_session* session);
XAML_RUNTIME_BRIDGE_API int mc_render_angle_surface(
    mc_session* session,
    xr_angle_surface* surface,
    unsigned char* destination,
    int destinationStride,
    int destinationCapacity);
XAML_RUNTIME_BRIDGE_API void xr_configure_logging(const char* filePath);
XAML_RUNTIME_BRIDGE_API void xr_log_info(const char* message);
XAML_RUNTIME_BRIDGE_API xr_element* xr_create_element(const char* type);
XAML_RUNTIME_BRIDGE_API void xr_destroy_element(xr_element* element);
XAML_RUNTIME_BRIDGE_API int xr_items_remove_item(xr_element* target);
XAML_RUNTIME_BRIDGE_API int xr_add_child(xr_element* parent, xr_element* child);
XAML_RUNTIME_BRIDGE_API int xr_set_attribute(
    xr_element* element,
    const char* name,
    const char* value);
XAML_RUNTIME_BRIDGE_API xr_element* xr_find_element(xr_element* root, const char* id);
XAML_RUNTIME_BRIDGE_API int xr_find_element_count(xr_element* root, const char* id);
XAML_RUNTIME_BRIDGE_API xr_element* xr_find_element_at(xr_element* root, const char* id, int index);
XAML_RUNTIME_BRIDGE_API int xr_add_storyboard_animation(xr_element* element, int trigger,
    const char* name, const char* const* keys, const char* const* values, int count);
XAML_RUNTIME_BRIDGE_API int xr_attach_animations(xr_element* root, xr_animation_controller* animations);
XAML_RUNTIME_BRIDGE_API int xr_set_page_transition(
    xr_element* root,
    xr_animation_controller* animations,
    const char* from,
    const char* to,
    int backward,
    int visible);
XAML_RUNTIME_BRIDGE_API int xr_add_storyboard_track(
    xr_element* element,
    int trigger,
    int property,
    float from,
    float to,
    int durationMilliseconds,
    int easing);
XAML_RUNTIME_BRIDGE_API int xr_add_visual_state_track(
    xr_element* scope,
    const char* groupName,
    const char* stateName,
    const char* targetName,
    int property,
    float from,
    float to,
    int durationMilliseconds,
    int easing);
XAML_RUNTIME_BRIDGE_API int xr_go_to_visual_state(
    xr_element* scope,
    const char* groupName,
    const char* stateName,
    int useTransitions);
XAML_RUNTIME_BRIDGE_API int xr_supported_attribute_count(const char* elementType);
XAML_RUNTIME_BRIDGE_API const char* xr_supported_attribute_name(
    const char* elementType,
    int index);
XAML_RUNTIME_BRIDGE_API int xr_supported_element_count(void);
XAML_RUNTIME_BRIDGE_API const char* xr_supported_element_name(int index);
XAML_RUNTIME_BRIDGE_API int xr_layout(xr_element* root, float width, float height);
XAML_RUNTIME_BRIDGE_API xr_element* xr_hit_test(xr_element* root, float x, float y);
XAML_RUNTIME_BRIDGE_API xr_element* xr_hit_test_visual(xr_element* root, float x, float y);
XAML_RUNTIME_BRIDGE_API int xr_hit_test_cursor_kind(xr_element* root, float x, float y);
XAML_RUNTIME_BRIDGE_API int xr_element_bounds(const xr_element* element, xr_rect* bounds);
XAML_RUNTIME_BRIDGE_API int xr_get_scroll_offsets(
    xr_element* root,
    const char* scroll_viewer_id,
    float* horizontal_offset,
    float* vertical_offset);
XAML_RUNTIME_BRIDGE_API int xr_set_scroll_offsets(
    xr_element* root,
    const char* scroll_viewer_id,
    float horizontal_offset,
    float vertical_offset);
XAML_RUNTIME_BRIDGE_API int xr_scroll_by(xr_element* root, float x, float y, float horizontalDelta, float verticalDelta);
XAML_RUNTIME_BRIDGE_API int xr_scroll_begin(
    xr_element* root,
    xr_animation_controller* animations,
    float x,
    float y);
XAML_RUNTIME_BRIDGE_API int xr_scroll_drag(xr_animation_controller* animations, float verticalDelta);
XAML_RUNTIME_BRIDGE_API void xr_scroll_end(xr_animation_controller* animations);
XAML_RUNTIME_BRIDGE_API int xr_set_render_offset_x(xr_element* element, float value);
XAML_RUNTIME_BRIDGE_API int xr_animate_render_offset_x(
    xr_element* element,
    xr_animation_controller* animations,
    float value,
    int duration_milliseconds);
XAML_RUNTIME_BRIDGE_API const char* xr_element_id(const xr_element* element);
XAML_RUNTIME_BRIDGE_API int xr_handle_tap(
    xr_element* element,
    xr_animation_controller* animations);
XAML_RUNTIME_BRIDGE_API int xr_handle_pointer_down(
    xr_element* element,
    xr_animation_controller* animations);
XAML_RUNTIME_BRIDGE_API int xr_handle_pointer_up(
    xr_element* element,
    xr_animation_controller* animations);
XAML_RUNTIME_BRIDGE_API xr_animation_controller* xr_create_animation_controller(void);
XAML_RUNTIME_BRIDGE_API void xr_destroy_animation_controller(
    xr_animation_controller* animations);
XAML_RUNTIME_BRIDGE_API xr_interaction_controller* xr_create_interaction_controller(void);
XAML_RUNTIME_BRIDGE_API void xr_destroy_interaction_controller(
    xr_interaction_controller* controller);
XAML_RUNTIME_BRIDGE_API int xr_interaction_pointer_down(
    xr_interaction_controller* controller,
    xr_element* root,
    xr_animation_controller* animations,
    float x,
    float y);
XAML_RUNTIME_BRIDGE_API int xr_interaction_pointer_move(
    xr_interaction_controller* controller,
    float x,
    float y);
XAML_RUNTIME_BRIDGE_API int xr_interaction_pointer_up(
    xr_interaction_controller* controller,
    xr_element* root,
    xr_animation_controller* animations,
    float x,
    float y,
    xr_interaction_result* result);
XAML_RUNTIME_BRIDGE_API int xr_interaction_scroll_wheel(
    xr_interaction_controller* controller,
    xr_element* root,
    float x,
    float y,
    float horizontal_delta,
    float vertical_delta);
XAML_RUNTIME_BRIDGE_API int xr_interaction_update(xr_interaction_controller* controller);
XAML_RUNTIME_BRIDGE_API int xr_set_animation_playback_rate(
    xr_animation_controller* animations,
    float playbackRate);
XAML_RUNTIME_BRIDGE_API int xr_update_animations(xr_animation_controller* animations);
XAML_RUNTIME_BRIDGE_API xr_angle_surface* xr_create_angle_surface(
    int width,
    int height,
    const char* fontPath,
    const char* resourceRoot);
XAML_RUNTIME_BRIDGE_API void xr_destroy_angle_surface(xr_angle_surface* surface);
XAML_RUNTIME_BRIDGE_API int xr_render_angle_surface(
    xr_angle_surface* surface,
    const xr_element* root,
    unsigned char* destination,
    int destinationStride,
    int destinationCapacity);
XAML_RUNTIME_BRIDGE_API int xr_render(
    const xr_element* root,
    xr_command* destination,
    int capacity);
XAML_RUNTIME_BRIDGE_API int xr_render_angle(
    const xr_element* root,
    const char* fontPath,
    int width,
    int height,
    const char* resourceRoot,
    unsigned char* destination,
    int destinationStride,
    int destinationCapacity);

#ifdef __cplusplus
}
#endif