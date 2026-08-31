#include "Controls/ButtonControl.h"

#include "Renderer/ControlRenderer.h"
#include <Helpers/platform/Android/Logging.h>

namespace mobileclock::ui {
    ButtonControl::ButtonControl(Element& element)
        : element(element) {
    }

    //
    // IControl
    //
    bool ButtonControl::HitTest(float x, float y) const {
        const Rect bounds = this->element.Bounds();
        return x >= bounds.x && x <= bounds.x + bounds.width
            && y >= bounds.y && y <= bounds.y + bounds.height;
    }

    bool ButtonControl::HandleTap(float x, float y) {
        LOG_FUNCTION_SCOPE("ButtonControl::HandleTap: x={}, y={}", x, y);
        if (!this->HitTest(x, y)) {
            return false;
        }
        this->isBlue = !this->isBlue;
        this->element.SetForeground(this->isBlue
            ? attr::Color{0.2f, 0.65f, 1.0f, 1.0f}
            : attr::Color{1.0f, 0.91f, 0.23f, 1.0f});
        return true;
    }

    void ButtonControl::Render(mobileclock::renderer::ControlRenderer& renderer) const {
        renderer.DrawOutline(this->element.Bounds(), this->element.Foreground());
        renderer.DrawText(this->element.Bounds(), this->element.Text(), this->element.Foreground());
    }

    Element& ButtonControl::ElementModel() {
        return this->element;
    }
}