using System.Runtime.InteropServices;
using System.Text;

namespace XamlPreviewer;

internal enum NativeCommandType {
    BeginClip,
    EndClip,
    Outline,
    RoundedRect,
    RoundedRectOutline,
    Text,
    Image
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativeRect {
    public float X;
    public float Y;
    public float Width;
    public float Height;
}

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
internal struct NativeInspectionResult {
    public int Line;
    public int Column;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 1024)]
    public string SourcePath;
    public NativeRect Bounds;
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativeColor {
    public float Red;
    public float Green;
    public float Blue;
    public float Alpha;
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativeInteractionResult {
    public int Kind;
    public int Direction;
    public IntPtr Target;
    public int ItemIndex;
}

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct NativeCommand {
    public int Type;
    public NativeRect Bounds;
    public NativeColor Color;
    public float Value;
    public fixed byte Text[512];
    public fixed byte Auxiliary[128];

    public string GetText() {
        fixed (byte* value = this.Text) {
            return NativeCommand.ReadUtf8(value, 512);
        }
    }

    public string GetAuxiliary() {
        fixed (byte* value = this.Auxiliary) {
            return NativeCommand.ReadUtf8(value, 128);
        }
    }

    public float GetAuxiliaryFloat() {
        fixed (byte* value = this.Auxiliary) {
            return *(float*)value;
        }
    }

    private static string ReadUtf8(byte* value, int capacity) {
        var length = 0;
        while (length < capacity && value[length] != 0) {
            ++length;
        }

        return Encoding.UTF8.GetString(value, length);
    }
}

internal static class NativeRuntime {
    private const string Library = "XamlPreviewer.NativeBridge.dll";

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_configure_logging")]
    public static extern void xr_configure_logging([MarshalAs(UnmanagedType.LPUTF8Str)] string filePath);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_create_session")]
    public static extern IntPtr mc_create_session(int width, int height);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_destroy_session")]
    public static extern void mc_destroy_session(IntPtr session);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_load_page")]
    public static extern int mc_load_page(IntPtr session, [MarshalAs(UnmanagedType.LPUTF8Str)] string page);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_current_page")]
    public static extern int mc_current_page(IntPtr session, [Out] StringBuilder page, int capacity);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_is_transitioning")]
    public static extern int mc_is_transitioning(IntPtr session);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_navigate_preview_route")]
    public static extern int mc_navigate_preview_route(
        IntPtr session,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string target);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_navigate_preview_route_path")]
    public static extern int mc_navigate_preview_route_path(
        IntPtr session,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string path);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_preview_route_graph")]
    public static extern int mc_preview_route_graph(IntPtr session, [Out] StringBuilder graph, int capacity);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_preview_page_title")]
    public static extern int mc_preview_page_title(
        IntPtr session,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string page,
        [Out] byte[] title,
        int capacity);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_apply_preview_scenario")]
    public static extern int mc_apply_preview_scenario(
        IntPtr session,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string page,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string json);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    public static extern int mc_reload_markup(
        IntPtr session,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string page,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string markup,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string sourcePath);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_resize")]
    public static extern int mc_resize(IntPtr session, int width, int height);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_set_animation_playback_rate")]
    public static extern int mc_set_animation_playback_rate(IntPtr session, float value);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_pointer_down")]
    public static extern int mc_pointer_down(IntPtr session, float x, float y);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_pointer_move")]
    public static extern int mc_pointer_move(IntPtr session, float x, float y);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_pointer_up")]
    public static extern int mc_pointer_up(IntPtr session, float x, float y);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_pointer_cancel")]
    public static extern int mc_pointer_cancel(IntPtr session);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_cursor_kind")]
    public static extern int mc_cursor_kind(IntPtr session, float x, float y);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_inspect")]
    public static extern int mc_inspect(IntPtr session, float x, float y, out NativeInspectionResult result);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_set_inspection_wireframe")]
    public static extern int mc_set_inspection_wireframe(
        IntPtr session,
        float thickness,
        int lineStyle,
        NativeColor color,
        NativeColor marginColor,
        NativeColor paddingColor);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_set_selected_wireframe")]
    public static extern int mc_set_selected_wireframe(
        IntPtr session,
        float thickness,
        int lineStyle,
        NativeColor color,
        NativeColor marginColor,
        NativeColor paddingColor);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_clear_inspection_wireframe")]
    public static extern int mc_clear_inspection_wireframe(IntPtr session);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_clear_selected_inspection_element")]
    public static extern int mc_clear_selected_inspection_element(IntPtr session);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_select_inspection_element")]
    public static extern int mc_select_inspection_element(
        IntPtr session,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string sourcePath,
        int line,
        int column);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_pin_inspection_element")]
    public static extern int mc_pin_inspection_element(IntPtr session);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_update")]
    public static extern int mc_update(IntPtr session);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "mc_render_angle_surface")]
    public static extern int mc_render_angle_surface(IntPtr session, IntPtr surface, [Out] byte[] pixels, int stride, int capacity);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_log_info")]
    public static extern void xr_log_info([MarshalAs(UnmanagedType.LPUTF8Str)] string message);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_create_element")]
    public static extern IntPtr xr_create_element([MarshalAs(UnmanagedType.LPUTF8Str)] string type);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_destroy_element")]
    public static extern void xr_destroy_element(IntPtr element);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_items_remove_item")]
    public static extern int xr_items_remove_item(IntPtr target);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_add_child")]
    public static extern int xr_add_child(IntPtr parent, IntPtr child);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_set_attribute")]
    public static extern int xr_set_attribute(
        IntPtr element,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string name,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string value);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr xr_find_element(
        IntPtr root,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string id);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    public static extern int xr_add_storyboard_animation(IntPtr element, int trigger,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string? name,
        [In] IntPtr[] keys, [In] IntPtr[] values, int count);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    public static extern int xr_attach_animations(IntPtr root, IntPtr animations);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    public static extern int xr_set_page_transition(
        IntPtr root,
        IntPtr animations,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string from,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string to,
        int backward,
        int visible);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_add_storyboard_track")]
    public static extern int xr_add_storyboard_track(
        IntPtr element,
        int trigger,
        int property,
        float from,
        float to,
        int durationMilliseconds,
        int easing);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    public static extern int xr_add_visual_state_track(
        IntPtr scope,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string groupName,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string stateName,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string targetName,
        int property, float from, float to, int durationMilliseconds, int easing);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)]
    public static extern int xr_go_to_visual_state(
        IntPtr scope,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string groupName,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string stateName,
        int useTransitions);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_supported_attribute_count")]
    public static extern int xr_supported_attribute_count(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string elementType);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_supported_attribute_name")]
    private static extern IntPtr xr_supported_attribute_name(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string elementType,
        int index);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_supported_element_count")]
    public static extern int xr_supported_element_count();

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_supported_element_name")]
    private static extern IntPtr xr_supported_element_name(int index);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_layout")]
    public static extern int xr_layout(IntPtr root, float width, float height);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_hit_test")]
    public static extern IntPtr xr_hit_test(IntPtr root, float x, float y);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_hit_test_visual")]
    public static extern IntPtr xr_hit_test_visual(IntPtr root, float x, float y);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_hit_test_cursor_kind")]
    public static extern int xr_hit_test_cursor_kind(IntPtr root, float x, float y);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_element_bounds")]
    public static extern int xr_element_bounds(IntPtr element, out NativeRect bounds);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_scroll_by")]
    public static extern int xr_scroll_by(IntPtr root, float x, float y, float horizontalDelta, float verticalDelta);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_scroll_begin")]
    public static extern int xr_scroll_begin(IntPtr root, IntPtr animations, float x, float y);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_scroll_drag")]
    public static extern int xr_scroll_drag(IntPtr animations, float verticalDelta);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_scroll_end")]
    public static extern void xr_scroll_end(IntPtr animations);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_set_render_offset_x")]
    public static extern int xr_set_render_offset_x(IntPtr element, float value);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_animate_render_offset_x")]
    public static extern int xr_animate_render_offset_x(
        IntPtr element,
        IntPtr animations,
        float value,
        int durationMilliseconds);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_element_id")]
    private static extern IntPtr xr_element_id(IntPtr element);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_handle_tap")]
    public static extern int xr_handle_tap(IntPtr element, IntPtr animations);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_handle_pointer_down")]
    public static extern int xr_handle_pointer_down(IntPtr element, IntPtr animations);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_handle_pointer_up")]
    public static extern int xr_handle_pointer_up(IntPtr element, IntPtr animations);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_create_animation_controller")]
    public static extern IntPtr xr_create_animation_controller();

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_destroy_animation_controller")]
    public static extern void xr_destroy_animation_controller(IntPtr animations);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_create_interaction_controller")]
    public static extern IntPtr xr_create_interaction_controller();

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_destroy_interaction_controller")]
    public static extern void xr_destroy_interaction_controller(IntPtr controller);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_interaction_pointer_down")]
    public static extern int xr_interaction_pointer_down(
        IntPtr controller, IntPtr root, IntPtr animations, float x, float y);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_interaction_pointer_move")]
    public static extern int xr_interaction_pointer_move(IntPtr controller, float x, float y);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_interaction_pointer_up")]
    public static extern int xr_interaction_pointer_up(
        IntPtr controller, IntPtr root, IntPtr animations, float x, float y, out NativeInteractionResult result);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_interaction_scroll_wheel")]
    public static extern int xr_interaction_scroll_wheel(
        IntPtr controller, IntPtr root, float x, float y, float horizontalDelta, float verticalDelta);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_interaction_update")]
    public static extern int xr_interaction_update(IntPtr controller);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_set_animation_playback_rate")]
    public static extern int xr_set_animation_playback_rate(IntPtr animations, float playbackRate);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_update_animations")]
    public static extern int xr_update_animations(IntPtr animations);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_create_angle_surface")]
    public static extern IntPtr xr_create_angle_surface(
        int width,
        int height,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string fontPath,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string resourceRoot);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_destroy_angle_surface")]
    public static extern void xr_destroy_angle_surface(IntPtr surface);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_render_angle_surface")]
    public static extern int xr_render_angle_surface(
        IntPtr surface,
        IntPtr root,
        [Out] byte[] pixels,
        int stride,
        int capacity);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_render")]
    public static extern int xr_render(IntPtr root, [Out] NativeCommand[]? commands, int capacity);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_render_angle")]
    public static extern int xr_render_angle(
        IntPtr root,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string fontPath,
        int width,
        int height,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string resourceRoot,
        [Out] byte[] pixels,
        int stride,
        int capacity);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "xr_last_error")]
    private static extern IntPtr xr_last_error();

    public static void Ensure(bool result) {
        if (!result) {
            throw new InvalidOperationException(NativeRuntime.GetLastError());
        }
    }

    public static string GetLastError() {
        return Marshal.PtrToStringUTF8(NativeRuntime.xr_last_error()) ?? "Unknown XamlRuntime error.";
    }


    public static string GetElementId(IntPtr element) {
        return Marshal.PtrToStringUTF8(NativeRuntime.xr_element_id(element)) ?? string.Empty;
    }

    public static string GetSupportedAttributeName(string elementType, int index) {
        return Marshal.PtrToStringUTF8(NativeRuntime.xr_supported_attribute_name(elementType, index)) ?? string.Empty;
    }

    public static string GetSupportedElementName(int index) {
        return Marshal.PtrToStringUTF8(NativeRuntime.xr_supported_element_name(index)) ?? string.Empty;
    }
}