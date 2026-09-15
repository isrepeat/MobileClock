#pragma once
#include <stdint.h>

#ifdef _WIN32
#define XAML_PREVIEWER_PLUGIN_API __declspec(dllexport)
#else
#define XAML_PREVIEWER_PLUGIN_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum {
    xaml_previewer_plugin_abi_version = 1,
};

// All strings crossing this ABI are UTF-8 and copied into caller-owned buffers.
// The opaque session belongs to the plugin that created it.
XAML_PREVIEWER_PLUGIN_API uint32_t xp_get_abi_version(void);
XAML_PREVIEWER_PLUGIN_API const char* xp_get_last_error(void);
XAML_PREVIEWER_PLUGIN_API int xp_get_initial_page_id(void* session, char* pageId, int capacity);
XAML_PREVIEWER_PLUGIN_API int xp_get_navigation_graph(void* session, char* graphJson, int capacity);
XAML_PREVIEWER_PLUGIN_API int xp_navigate(void* session, const char* transitionIds);

#ifdef __cplusplus
}
#endif