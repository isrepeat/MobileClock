#include "Renderer/ControlRenderer.h"

namespace mobileclock::renderer {
    ControlRenderer::ControlRenderer(DrawOutlineCallback drawOutline, DrawRoundedRectCallback drawRoundedRect,
        DrawToggleSwitchCallback drawToggleSwitch, DrawTextCallback drawText)
        : drawOutline(std::move(drawOutline))
        , drawRoundedRect(std::move(drawRoundedRect))
        , drawToggleSwitch(std::move(drawToggleSwitch))
        , drawText(std::move(drawText)) {
    }

    //
    // API
    //
    void ControlRenderer::DrawRoundedRect(const xaml::Rect& bounds,
        xaml::attr::Color color, float cornerRadius) const {
        this->drawRoundedRect(bounds, color, cornerRadius);
    }

    void ControlRenderer::DrawToggleSwitch(const xaml::Rect& bounds, bool isOn) const {
        this->drawToggleSwitch(bounds, isOn);
    }

    void ControlRenderer::DrawOutline(const xaml::Rect& bounds,
        xaml::attr::Color color) const {
        this->drawOutline(bounds, color);
    }

    void ControlRenderer::DrawText(const xaml::Rect& bounds, std::string_view text,
        xaml::attr::Color color) const {
        this->drawText(bounds, text, color);
    }
}