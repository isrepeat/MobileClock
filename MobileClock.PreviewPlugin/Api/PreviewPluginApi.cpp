#include "PreviewPluginApi.h"

#include "../Bridge/TextBuffer.h"

#include <cstring>
#include <format>
#include <stdexcept>
#include <string>

namespace mobileclock::preview {
    const char* PreviewPluginApi::LastError() {
        return xaml::bridge::lastError.c_str();
    }

    uint32_t PreviewPluginApi::AbiVersion() {
        return AndroidAppPreviewerPluginSDK::xaml_previewer_plugin_abi_version;
    }

    bool PreviewPluginApi::WritePluginInfo(char* destination, int capacity) {
        if (destination == nullptr || capacity <= 0) {
            throw std::invalid_argument("Plugin-info buffer and positive capacity are required");
        }
        const std::string pluginInfo = std::format(
            R"({{"applicationId":"MobileClock","displayName":"MobileClock","resourceRootRelativePath":"Resources","sourceMarkupDirectory":"{}","sourceEntryMarkupPath":"{}","sourceControlsDirectory":"{}"}})",
            MOBILECLOCK_PREVIEW_SOURCE_MARKUP_DIRECTORY,
            MOBILECLOCK_PREVIEW_SOURCE_ENTRY_MARKUP_PATH,
            MOBILECLOCK_PREVIEW_SOURCE_CONTROLS_DIRECTORY);
        bridge::TextBuffer::Write(
            pluginInfo,
            destination,
            static_cast<size_t>(capacity),
            "Plugin-info buffer is too small");
        return true;
    }

    AndroidAppPreviewerPluginSDK::xp_session* PreviewPluginApi::CreateSession(int width, int height) {
        return new AndroidAppPreviewerPluginSDK::xp_session(width, height);
    }

    void PreviewPluginApi::DestroySession(AndroidAppPreviewerPluginSDK::xp_session* session) {
        delete session;
    }
}