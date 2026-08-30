#include "Controls/ButtonControl.h"

#include "Renderer/ControlRenderer.h"
#include <Helpers/platform/Android/Logging.h>

namespace mobileclock::ui {
    ButtonControl::ButtonControl(const Element& element)
        : _bounds(element.bounds()), _foreground(element.foreground()), _text(element.text()) {
    }

    bool ButtonControl::handleTap(float x, float y) {
        LOG_FUNCTION_SCOPE("ButtonControl::handleTap: x={}, y={}", x, y);
        const bool tapped = x >= _bounds.x && x <= _bounds.x + _bounds.width
            && y >= _bounds.y && y <= _bounds.y + _bounds.height;
        if (!tapped) return false;
        _isBlue = !_isBlue;
        _foreground = _isBlue ? Color{0.2f, 0.65f, 1.0f, 1.0f} : Color{1.0f, 0.91f, 0.23f, 1.0f};
        return true;
    }

    void ButtonControl::render(mobileclock::renderer::ControlRenderer& renderer) const {
        renderer.drawOutline(_bounds, _foreground);
        renderer.drawText(_text, _foreground);
    }
}