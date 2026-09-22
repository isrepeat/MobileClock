#pragma once
#include <AndroidAppPreviewer.PluginSDK/AndroidAppPreviewerPlugin.h>

namespace mobileclock::preview::api {
    using namespace AndroidAppPreviewerPluginSDK;
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
}