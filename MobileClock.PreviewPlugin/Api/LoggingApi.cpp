#include "LoggingApi.h"

#include <AndroidAppPreviewer.PluginSDK/AndroidAppPreviewerPlugin.h>
#include <Helpers.Logging/Logging.h>
#include <HelpersNew/Geometry/ContainsPoint.h>
#include <XamlRuntime/InteractionController.h>
#include <XamlRuntime/ScrollController.h>
#include <XamlRuntime/ElementBuilder.h>
#include <XamlRuntime/RenderEngine.h>
#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Animation.h>
#include <XamlRuntime/Input.h>

#include "MobileClock.Presentation/Core/PreviewSession.h"

#include "../Rendering/AngleRenderSurface.h"
#include "../Rendering/RecordingBackend.h"
#include "../Session/PreviewNavigationController.h"
#include "../Session/PreviewSession.h"
#include "../Bridge/Diagnostic.h"
#include "../Bridge/TextBuffer.h"
#include "PreviewPluginApi.h"

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

namespace mobileclock::preview::api {
    using namespace AndroidAppPreviewerPluginSDK;
    void LoggingApi::xp_configure_logging(const char* filePath) {
        utility_helpers::logging::Configure({
            filePath == nullptr ? std::filesystem::path{} : std::filesystem::path(filePath),
        });
        utility_helpers::logging::Initialize("AndroidAppPreviewer");
        LOG_INFO("MobileClock.PreviewPlugin", "Logging initialized");
    }

    void LoggingApi::xp_log_info(const char* message) {
        if (message != nullptr) {
            LOG_INFO("AndroidAppPreviewer.Interaction", "{}", message);
        }
    }
} // namespace mobileclock::preview::api