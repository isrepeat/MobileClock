#include <AndroidAppPreviewer.PluginSDK/AndroidAppPreviewerPlugin.h>

#include "Api/XamlCompletionApi.h"
#include "Api/InteractionApi.h"
#include "Api/RenderingApi.h"
#include "Api/MetadataApi.h"
#include "Api/SessionApi.h"
#include "Api/ElementApi.h"
#include "Api/LoggingApi.h"

namespace _details {
    using namespace AndroidAppPreviewerPluginSDK;
    using namespace mobileclock::preview::api;

    const xp_metadata_api metadataApi{
        android_app_previewer_plugin_api_version,
        sizeof(xp_metadata_api),
        &MetadataApi::xp_get_abi_version,
        &MetadataApi::xp_last_error,
        &MetadataApi::xp_get_plugin_info,
        &MetadataApi::xp_get_initial_page_id,
        &MetadataApi::xp_get_navigation_graph,
        &MetadataApi::xp_navigate
    };

    const xp_session_api sessionApi{
        android_app_previewer_plugin_api_version,
        sizeof(xp_session_api),
        &SessionApi::xp_create_session,
        &SessionApi::xp_destroy_session,
        &SessionApi::xp_session_load_page,
        &SessionApi::xp_session_current_page,
        &SessionApi::xp_session_is_transitioning,
        &SessionApi::xp_session_navigate_preview_route,
        &SessionApi::xp_session_navigate_preview_route_path,
        &SessionApi::xp_session_preview_route_graph,
        &SessionApi::xp_session_preview_page_title,
        &SessionApi::xp_session_apply_preview_scenario,
        &SessionApi::xp_session_export_preview_state,
        &SessionApi::xp_session_can_save_preview_state,
        &SessionApi::xp_session_reload_markup,
        &SessionApi::xp_session_resize,
        &SessionApi::xp_session_set_animation_playback_rate,
        &SessionApi::xp_session_set_status,
        &SessionApi::xp_session_pointer_down,
        &SessionApi::xp_session_pointer_move,
        &SessionApi::xp_session_pointer_up,
        &SessionApi::xp_session_pointer_cancel,
        &SessionApi::xp_session_cursor_kind,
        &SessionApi::xp_session_inspect,
        &SessionApi::xp_session_set_inspection_wireframe,
        &SessionApi::xp_session_set_selected_wireframe,
        &SessionApi::xp_session_clear_inspection_wireframe,
        &SessionApi::xp_session_clear_selected_inspection_element,
        &SessionApi::xp_session_select_inspection_element,
        &SessionApi::xp_session_pin_inspection_element,
        &SessionApi::xp_session_update,
        &SessionApi::xp_session_render_angle_surface
    };

    const xp_element_api elementApi{
        android_app_previewer_plugin_api_version,
        sizeof(xp_element_api),
        &ElementApi::xp_create_element,
        &ElementApi::xp_destroy_element,
        &ElementApi::xp_items_remove_item,
        &ElementApi::xp_add_child,
        &ElementApi::xp_set_attribute,
        &ElementApi::xp_find_element,
        &ElementApi::xp_find_element_count,
        &ElementApi::xp_find_element_at,
        &ElementApi::xp_layout,
        &ElementApi::xp_hit_test,
        &ElementApi::xp_hit_test_visual,
        &ElementApi::xp_hit_test_cursor_kind,
        &ElementApi::xp_element_bounds,
        &ElementApi::xp_element_id,
        &ElementApi::xp_get_scroll_offsets,
        &ElementApi::xp_set_scroll_offsets,
        &ElementApi::xp_scroll_by
    };

    const xp_interaction_api interactionApi{
        android_app_previewer_plugin_api_version,
        sizeof(xp_interaction_api),
        &InteractionApi::xp_add_storyboard_animation,
        &InteractionApi::xp_attach_animations,
        &InteractionApi::xp_set_page_transition,
        &InteractionApi::xp_add_storyboard_track,
        &InteractionApi::xp_add_visual_state_track,
        &InteractionApi::xp_go_to_visual_state,
        &InteractionApi::xp_scroll_begin,
        &InteractionApi::xp_scroll_drag,
        &InteractionApi::xp_scroll_end,
        &InteractionApi::xp_set_render_offset_x,
        &InteractionApi::xp_animate_render_offset_x,
        &InteractionApi::xp_handle_tap,
        &InteractionApi::xp_handle_pointer_down,
        &InteractionApi::xp_handle_pointer_up,
        &InteractionApi::xp_create_animation_controller,
        &InteractionApi::xp_destroy_animation_controller,
        &InteractionApi::xp_create_interaction_controller,
        &InteractionApi::xp_destroy_interaction_controller,
        &InteractionApi::xp_interaction_pointer_down,
        &InteractionApi::xp_interaction_pointer_move,
        &InteractionApi::xp_interaction_pointer_up,
        &InteractionApi::xp_interaction_scroll_wheel,
        &InteractionApi::xp_interaction_update,
        &InteractionApi::xp_set_animation_playback_rate,
        &InteractionApi::xp_update_animations
    };

    const xp_rendering_api renderingApi{
        android_app_previewer_plugin_api_version,
        sizeof(xp_rendering_api),
        &RenderingApi::xp_create_angle_surface,
        &RenderingApi::xp_destroy_angle_surface,
        &RenderingApi::xp_render_angle_surface,
        &RenderingApi::xp_render,
        &RenderingApi::xp_render_angle
    };

    const xp_xaml_completion_api xamlCompletionApi{
        android_app_previewer_plugin_api_version,
        sizeof(xp_xaml_completion_api),
        &XamlCompletionApi::xp_supported_attribute_count,
        &XamlCompletionApi::xp_supported_attribute_name,
        &XamlCompletionApi::xp_supported_element_count,
        &XamlCompletionApi::xp_supported_element_name
    };

    const xp_logging_api loggingApi{
        android_app_previewer_plugin_api_version,
        sizeof(xp_logging_api),
        &LoggingApi::xp_configure_logging,
        &LoggingApi::xp_log_info
    };

    const xp_plugin_api pluginApi{
        android_app_previewer_plugin_api_version,
        sizeof(xp_plugin_api),
        metadataApi,
        sessionApi,
        elementApi,
        interactionApi,
        renderingApi,
        xamlCompletionApi,
        loggingApi
    };
} // namespace _details

namespace AndroidAppPreviewerPluginSDK {
    extern "C" ANDROID_APP_PREVIEWER_PLUGIN_API const xp_plugin_api* XP_PLUGIN_CALL xp_get_api(uint32_t requestedVersion) {
        return requestedVersion == android_app_previewer_plugin_api_version ? &_details::pluginApi : nullptr;
    }
} // namespace AndroidAppPreviewerPluginSDK