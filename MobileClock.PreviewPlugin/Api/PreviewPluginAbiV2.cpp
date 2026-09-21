#include <AndroidAppPreviewer.PluginSDK/AndroidAppPreviewerPlugin.h>

#include "PreviewPluginLegacyApi.h"

namespace {
    using namespace AndroidAppPreviewerPluginSDK;

    const xp_metadata_api metadataApi{
        android_app_previewer_plugin_api_version,
        sizeof(xp_metadata_api),
        &xp_get_abi_version,
        &xp_last_error,
        &xp_get_plugin_info,
        &xp_get_initial_page_id,
        &xp_get_navigation_graph,
        &xp_navigate
    };

    const xp_session_api sessionApi{
        android_app_previewer_plugin_api_version,
        sizeof(xp_session_api),
        &xp_create_session,
        &xp_destroy_session,
        &xp_session_load_page,
        &xp_session_current_page,
        &xp_session_is_transitioning,
        &xp_session_navigate_preview_route,
        &xp_session_navigate_preview_route_path,
        &xp_session_preview_route_graph,
        &xp_session_preview_page_title,
        &xp_session_apply_preview_scenario,
        &xp_session_export_preview_state,
        &xp_session_can_save_preview_state,
        &xp_session_reload_markup,
        &xp_session_resize,
        &xp_session_set_animation_playback_rate,
        &xp_session_set_status,
        &xp_session_pointer_down,
        &xp_session_pointer_move,
        &xp_session_pointer_up,
        &xp_session_pointer_cancel,
        &xp_session_cursor_kind,
        &xp_session_inspect,
        &xp_session_set_inspection_wireframe,
        &xp_session_set_selected_wireframe,
        &xp_session_clear_inspection_wireframe,
        &xp_session_clear_selected_inspection_element,
        &xp_session_select_inspection_element,
        &xp_session_pin_inspection_element,
        &xp_session_update,
        &xp_session_render_angle_surface
    };

    const xp_element_api elementApi{
        android_app_previewer_plugin_api_version,
        sizeof(xp_element_api),
        &xp_create_element,
        &xp_destroy_element,
        &xp_items_remove_item,
        &xp_add_child,
        &xp_set_attribute,
        &xp_find_element,
        &xp_find_element_count,
        &xp_find_element_at,
        &xp_layout,
        &xp_hit_test,
        &xp_hit_test_visual,
        &xp_hit_test_cursor_kind,
        &xp_element_bounds,
        &xp_element_id,
        &xp_get_scroll_offsets,
        &xp_set_scroll_offsets,
        &xp_scroll_by
    };

    const xp_interaction_api interactionApi{
        android_app_previewer_plugin_api_version,
        sizeof(xp_interaction_api),
        &xp_add_storyboard_animation,
        &xp_attach_animations,
        &xp_set_page_transition,
        &xp_add_storyboard_track,
        &xp_add_visual_state_track,
        &xp_go_to_visual_state,
        &xp_scroll_begin,
        &xp_scroll_drag,
        &xp_scroll_end,
        &xp_set_render_offset_x,
        &xp_animate_render_offset_x,
        &xp_handle_tap,
        &xp_handle_pointer_down,
        &xp_handle_pointer_up,
        &xp_create_animation_controller,
        &xp_destroy_animation_controller,
        &xp_create_interaction_controller,
        &xp_destroy_interaction_controller,
        &xp_interaction_pointer_down,
        &xp_interaction_pointer_move,
        &xp_interaction_pointer_up,
        &xp_interaction_scroll_wheel,
        &xp_interaction_update,
        &xp_set_animation_playback_rate,
        &xp_update_animations
    };

    const xp_rendering_api renderingApi{
        android_app_previewer_plugin_api_version,
        sizeof(xp_rendering_api),
        &xp_create_angle_surface,
        &xp_destroy_angle_surface,
        &xp_render_angle_surface,
        &xp_render,
        &xp_render_angle
    };

    const xp_xaml_completion_api xamlCompletionApi{
        android_app_previewer_plugin_api_version,
        sizeof(xp_xaml_completion_api),
        &xp_supported_attribute_count,
        &xp_supported_attribute_name,
        &xp_supported_element_count,
        &xp_supported_element_name
    };

    const xp_logging_api loggingApi{
        android_app_previewer_plugin_api_version,
        sizeof(xp_logging_api),
        &xp_configure_logging,
        &xp_log_info
    };

    const xp_plugin_api pluginApi{
        android_app_previewer_plugin_api_version,
        sizeof(xp_plugin_api),
        metadataApi,
        sessionApi,
        elementApi,
        xamlCompletionApi,
        interactionApi,
        renderingApi,
        loggingApi
    };
}

namespace AndroidAppPreviewerPluginSDK {
    extern "C" ANDROID_APP_PREVIEWER_PLUGIN_API const xp_plugin_api* XP_PLUGIN_CALL xp_get_api(uint32_t requestedVersion) {
        return requestedVersion == android_app_previewer_plugin_api_version ? &pluginApi : nullptr;
    }
}