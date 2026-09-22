#include "MetadataApi.h"

#include "SessionApi.h"

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
#include "../Session/PreviewSessionApi.h"
#include "../Session/PreviewSession.h"
#include "../Bridge/Diagnostic.h"
#include "../Bridge/PreviewPluginSdkTypes.h"
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
    const char* MetadataApi::xp_last_error(void) {
        return mobileclock::preview::api::PreviewPluginApi::LastError();
    }

    uint32_t MetadataApi::xp_get_abi_version(void) {
        return mobileclock::preview::api::PreviewPluginApi::AbiVersion();
    }

    int MetadataApi::xp_get_plugin_info(
        char* pluginInfoJson,
        int capacity
    ) {
        try {
            mobileclock::preview::bridge::LastError().clear();
            return mobileclock::preview::api::PreviewPluginApi::WritePluginInfo(pluginInfoJson, capacity) ? 1 : 0;
        } catch (const std::exception& error) {
            mobileclock::preview::bridge::LastError() = error.what();
            return 0;
        }
    }

    int MetadataApi::xp_get_initial_page_id(
        void* session,
        char* pageId,
        int capacity
    ) {
        return mobileclock::preview::api::SessionApi::xp_session_current_page(static_cast<xp_session*>(session), pageId, capacity);
    }

    int MetadataApi::xp_get_navigation_graph(
        void* session,
        char* graphJson,
        int capacity
    ) {
        try {
            mobileclock::preview::bridge::LastError().clear();
            if (session == nullptr || graphJson == nullptr || capacity <= 0) {
                throw std::invalid_argument("Session, graph buffer and positive capacity are required");
            }
            const std::string json = static_cast<xp_session*>(session)->value.Navigation().BuildGraphJson();
            mobileclock::preview::bridge::TextBuffer::Write(
                json,
                graphJson,
                static_cast<size_t>(capacity),
                "Navigation graph buffer is too small");
            return 1;
        } catch (const std::exception& error) {
            mobileclock::preview::bridge::LastError() = error.what();
            return 0;
        }
    }

    int MetadataApi::xp_navigate(
        void* session,
        const char* navigationRequestJson
    ) {
        try {
            mobileclock::preview::bridge::LastError().clear();
            if (session == nullptr || navigationRequestJson == nullptr) {
                throw std::invalid_argument("Session and navigation request are required");
            }
            const std::vector<std::string> ids = mobileclock::preview::session::PreviewSession::ParseNavigationTransitionIds(navigationRequestJson);
            std::vector<std::string_view> transitionIds;
            transitionIds.reserve(ids.size());
            for (const std::string& id : ids) {
                transitionIds.push_back(id);
            }
            if (!static_cast<xp_session*>(session)->value.Navigation().Navigate(
                transitionIds,
                mobileclock::preview::bridge::LastError())) {
                return 0;
            }
            return 1;
        } catch (const std::exception& error) {
            mobileclock::preview::bridge::LastError() = error.what();
            return 0;
        }
    }
} // namespace mobileclock::preview::api