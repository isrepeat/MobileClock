#include "Renderer/ControlRenderer.h"

namespace mobileclock::renderer {
    ControlRenderer::ControlRenderer(DrawOutline drawOutline, DrawText drawText)
        : _drawOutline(std::move(drawOutline)), _drawText(std::move(drawText)) {
    }

    void ControlRenderer::drawOutline(const mobileclock::ui::Rect& bounds,
        mobileclock::ui::Color color) const {
        _drawOutline(bounds, color);
    }

    void ControlRenderer::drawText(std::string_view text, mobileclock::ui::Color color) const {
        _drawText(text, color);
    }
}