#include "XamlCompletionApi.h"

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
    int XamlCompletionApi::xp_supported_attribute_count(const char* elementType) {
        try {
            mobileclock::preview::bridge::LastError().clear();
            if (elementType == nullptr) {
                throw std::invalid_argument("element type is required");
            }
            if (std::string_view(elementType) == "columnDefinition" || std::string_view(elementType) == "rowDefinition") {
                return 1;
            }
            return static_cast<int>(xaml::SupportedAttributeNames(xaml::ParseElementType(elementType)).size());
        } catch (const std::exception& error) {
            mobileclock::preview::bridge::LastError() = error.what();
            return 0;
        }
    }

    const char* XamlCompletionApi::xp_supported_attribute_name(
        const char* elementType,
        int index
    ) {
        try {
            mobileclock::preview::bridge::LastError().clear();
            if (elementType == nullptr || index < 0) {
                throw std::invalid_argument("element type and non-negative index are required");
            }
            if (std::string_view(elementType) == "columnDefinition" && index == 0) {
                return "width";
            }
            if (std::string_view(elementType) == "rowDefinition" && index == 0) {
                return "height";
            }
            const std::vector<std::string_view> names = xaml::SupportedAttributeNames(
                xaml::ParseElementType(elementType));
            if (index >= static_cast<int>(names.size())) {
                throw std::out_of_range("attribute index is out of range");
            }
            return names[static_cast<size_t>(index)].data();
        } catch (const std::exception& error) {
            mobileclock::preview::bridge::LastError() = error.what();
            return "";
        }
    }

    int XamlCompletionApi::xp_supported_element_count(void) {
        return 18;
    }

    const char* XamlCompletionApi::xp_supported_element_name(int index) {
        static constexpr std::string_view names[]{
            "Page", "StackPanel", "Grid", "Border", "TextBlock", "Button", "IconButton", "ToggleSwitch",
            "ScrollViewer", "Image", "SvgImage", "ListView", "ListView.ItemTemplate", "DataTemplate",
            "columnDefinitions", "columnDefinition", "rowDefinitions", "rowDefinition"
        };
        return index >= 0 && index < static_cast<int>(std::size(names)) ? names[index].data() : "";
    }
} // namespace mobileclock::preview::api