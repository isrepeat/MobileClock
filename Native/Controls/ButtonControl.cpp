#include "Controls/ButtonControl.h"

#include "Renderer/ControlRenderer.h"
#include <Helpers/platform/Android/Logging.h>

namespace mobileclock::ui {
    ButtonControl::ButtonControl(const Element& element)
        : bounds(element.Bounds())
        , foreground(element.Foreground())
        , text(element.Text()) {
    }

    bool ButtonControl::HandleTap(float x, float y) {
        LOG_FUNCTION_SCOPE("ButtonControl::HandleTap: x={}, y={}", x, y);
        const bool tapped = x >= this->bounds.x && x <= this->bounds.x + this->bounds.width
            && y >= this->bounds.y && y <= this->bounds.y + this->bounds.height;
        if (!tapped) {
            return false;
        }
        this->isBlue = !this->isBlue;
        this->foreground = this->isBlue ? attr::Color{0.2f, 0.65f, 1.0f, 1.0f} : attr::Color{1.0f, 0.91f, 0.23f, 1.0f};
        return true;
    }

    void ButtonControl::Render(mobileclock::renderer::ControlRenderer& renderer) const {
        renderer.DrawOutline(this->bounds, this->foreground);
        renderer.DrawText(this->text, this->foreground);
    }
}