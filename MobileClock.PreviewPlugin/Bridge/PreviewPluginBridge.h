#pragma once
#include <AndroidAppPreviewer.PluginSDK/AndroidAppPreviewerPlugin.h>
#include <XamlRuntime/InteractionController.h>
#include <XamlRuntime/ScrollController.h>
#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Animation.h>

#include "MobileClock.Presentation/Core/PreviewSession.h"

#include "../Rendering/AngleRenderSurface.h"
#include "../Session/PreviewSession.h"

#include <string_view>
#include <memory>
#include <string>

namespace xaml::bridge {
    extern thread_local std::string lastError;
}

namespace AndroidAppPreviewerPluginSDK {
    struct xp_animation_controller {
        mobileclock::presentation::core::PreviewSession value;
        xaml::ScrollController scrollController;
    };

    struct xp_interaction_controller {
        xaml::InteractionController value;
    };

    struct xp_angle_surface {
        explicit xp_angle_surface(
            int width,
            int height,
            const char* fontPath,
            const char* resourceRoot);

        int width;
        int height;
        mobileclock::preview::rendering::AngleRenderSurface value;
    };

    struct xp_session {
        explicit xp_session(int width, int height);

        mobileclock::preview::PreviewSession value;
        xaml::Element* inspectionElement = nullptr;
        std::weak_ptr<void> inspectionElementLifetime;
        xaml::Element* selectedElement = nullptr;
        std::weak_ptr<void> selectedElementLifetime;
        xaml::attr::Wireframe inspectionWireframe{
            3.0f,
            xaml::attr::WireframeLineStyle::solid,
            {0.878f, 0.322f, 0.322f, 1.0f},
        };
        xaml::attr::Wireframe selectedWireframe{
            3.0f,
            xaml::attr::WireframeLineStyle::solid,
            {0.30f, 0.64f, 1.0f, 1.0f},
        };
    };
}

namespace mobileclock::preview::_details {
    void ClearInspectionWireframe(AndroidAppPreviewerPluginSDK::xp_session& session);
    void ClearSelectedWireframe(AndroidAppPreviewerPluginSDK::xp_session& session);
    void SetInspectionWireframe(
        AndroidAppPreviewerPluginSDK::xp_session& session,
        xaml::Element& element);
    void SetSelectedWireframe(
        AndroidAppPreviewerPluginSDK::xp_session& session,
        xaml::Element& element);
}

namespace xaml::bridge::_details {
    bool SameSourcePath(std::string_view left, std::string_view right);
    Element* FindElement(Element& element, std::string_view id);
    int CountElements(const Element& element, std::string_view id);
    Element* FindElementAt(Element& element, std::string_view id, int& index);
    Element* FindElementAtSource(Element& element, std::string_view sourcePath, int line, int column);
    Element* HitTestVisual(Element& element, float x, float y, float offsetX = 0.0f, float offsetY = 0.0f);
    Element* FindScrollViewer(Element& root, float x, float y);
    bool RemoveItem(Element& target);
}