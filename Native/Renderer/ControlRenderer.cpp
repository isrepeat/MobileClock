#include "Renderer/ControlRenderer.h"

namespace mobileclock::renderer {
    ControlRenderer::ControlRenderer(
        DrawOutlineCallback drawOutline,
        DrawRoundedRectCallback drawRoundedRect,
        DrawRoundedRectOutlineCallback drawRoundedRectOutline,
        DrawTextCallback drawText,
        DrawImageCallback drawImage,
        BeginClipCallback beginClip,
        EndClipCallback endClip)
        : drawOutline(std::move(drawOutline))
        , drawRoundedRect(std::move(drawRoundedRect))
        , drawRoundedRectOutline(std::move(drawRoundedRectOutline))
        , drawText(std::move(drawText))
        , drawImage(std::move(drawImage))
        , beginClip(std::move(beginClip))
        , endClip(std::move(endClip)) {
    }

    //
    // IRenderBackend
    //
    void ControlRenderer::BeginClip(const xaml::Rect& bounds) {
        this->beginClip(bounds);
    }

    void ControlRenderer::EndClip() {
        this->endClip();
    }

    void ControlRenderer::DrawOutline(
        const xaml::Rect& bounds,
        xaml::attr::Color color) {
        this->drawOutline(bounds, color);
    }

    void ControlRenderer::DrawRoundedRect(
        const xaml::Rect& bounds,
        xaml::attr::Color color,
        float cornerRadius) {
        this->drawRoundedRect(bounds, color, cornerRadius);
    }

    void ControlRenderer::DrawRoundedRectOutline(
        const xaml::Rect& bounds,
        xaml::attr::Color color,
        float cornerRadius,
        float thickness) {
        this->drawRoundedRectOutline(bounds, color, cornerRadius, thickness);
    }

    void ControlRenderer::DrawText(
        const xaml::Rect& bounds,
        std::string_view text,
        xaml::attr::Color color,
        float fontSize,
        std::string_view fontWeight) {
        this->drawText(bounds, text, color, fontSize, fontWeight);
    }

    void ControlRenderer::DrawImage(
        const xaml::Rect& bounds,
        std::string_view source,
        xaml::attr::Color tint) {
        this->drawImage(bounds, source, tint);
    }
}