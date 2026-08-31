#include "Renderer/ControlRenderer.h"

namespace mobileclock::renderer {
    ControlRenderer::ControlRenderer(DrawOutlineCallback drawOutline, DrawTextCallback drawText)
        : _drawOutline(std::move(drawOutline)), _drawText(std::move(drawText)) {
    }

    void ControlRenderer::DrawOutline(const mobileclock::ui::Rect& bounds,
        mobileclock::ui::Color color) const {
        _drawOutline(bounds, color);
    }

    void ControlRenderer::DrawText(std::string_view text, mobileclock::ui::Color color) const {
        _drawText(text, color);
    }
}