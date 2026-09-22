#pragma once
#include <AndroidAppPreviewer.PluginSDK/AndroidAppPreviewerPlugin.h>

namespace mobileclock::preview::api {
    using namespace AndroidAppPreviewerPluginSDK;
    class LoggingApi final {
    public:
        static void xp_configure_logging(const char* filePath);
        static void xp_log_info(const char* message);
    };
}