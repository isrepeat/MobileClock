#include "Controls/ControlChrome.h"

#include "Renderer/ControlRenderer.h"

#include <XamlRuntime/XamlLayout.h>

#include <algorithm>

namespace mobileclock::ui {
    void RenderControlChrome(
        const xaml::Element& element,
        mobileclock::renderer::ControlRenderer& renderer) {
        const xaml::attr::Thickness borderThickness = element.BorderThickness();
        const bool hasBorder = borderThickness.left > 0.0f || borderThickness.right > 0.0f
            || borderThickness.top > 0.0f || borderThickness.bottom > 0.0f;
        if (!hasBorder) {
            renderer.DrawRoundedRect(element.Bounds(), element.Background(), element.CornerRadius());
            return;
        }

        const xaml::Rect bounds = element.Bounds();
        if (element.Background().alpha <= 0.0f) {
            renderer.DrawRoundedRectOutline(
                bounds,
                element.BorderColor(),
                element.CornerRadius(),
                std::max({
                    borderThickness.left,
                    borderThickness.right,
                    borderThickness.top,
                    borderThickness.bottom,
                }));
            return;
        }
        renderer.DrawRoundedRect(bounds, element.BorderColor(), element.CornerRadius());
        const xaml::Rect innerBounds{
            bounds.x + borderThickness.left,
            bounds.y + borderThickness.top,
            std::max(0.0f, bounds.width - borderThickness.left - borderThickness.right),
            std::max(0.0f, bounds.height - borderThickness.top - borderThickness.bottom),
        };
        const float innerRadius = element.CornerRadius() - std::max({
            borderThickness.left,
            borderThickness.right,
            borderThickness.top,
            borderThickness.bottom,
        });
        renderer.DrawRoundedRect(innerBounds, element.Background(), std::max(0.0f, innerRadius));
    }
}