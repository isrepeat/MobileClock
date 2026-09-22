#pragma once
#include <AndroidAppPreviewer.PluginSDK/AndroidAppPreviewerPlugin.h>
#include <XamlRuntime/Animation.h>
#include <XamlRuntime/InteractionController.h>
#include <XamlRuntime/ScrollController.h>
#include <XamlRuntime/XamlLayout.h>

#include "MobileClock.Presentation/Core/PreviewSession.h"

#include "../Rendering/AngleRenderSurface.h"
#include "../Session/PreviewSession.h"

#include <memory>

namespace AndroidAppPreviewerPluginSDK {
    struct xp_session {
        explicit xp_session(int width, int height);

        mobileclock::preview::session::PreviewSession value;
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

    struct xp_animation_controller {
        mobileclock::presentation::core::PreviewSession value;
        xaml::ScrollController scrollController;
    };

    struct xp_interaction_controller {
        xaml::InteractionController value;
    };
}