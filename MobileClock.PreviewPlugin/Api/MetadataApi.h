#pragma once
#include <AndroidAppPreviewer.PluginSDK/AndroidAppPreviewerPlugin.h>

namespace mobileclock::preview::api {
    using namespace AndroidAppPreviewerPluginSDK;

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
}