#include "Controls/ToggleSwitchControl.h"

#include "Renderer/ControlRenderer.h"

namespace mobileclock::ui {
    ToggleSwitchControl::ToggleSwitchControl(const Element& element)
        : bounds(element.Bounds())
        , isOn(element.IsOn()) {
    }

    bool ToggleSwitchControl::HitTest(float x, float y) const {
        return x >= this->bounds.x && x <= this->bounds.x + this->bounds.width
            && y >= this->bounds.y && y <= this->bounds.y + this->bounds.height;
    }

    bool ToggleSwitchControl::HandleTap(float x, float y) {
        if (!this->HitTest(x, y)) {
            return false;
        }
        this->isOn = !this->isOn;
        return true;
    }

    void ToggleSwitchControl::Render(mobileclock::renderer::ControlRenderer& renderer) const {
        renderer.DrawToggleSwitch(this->bounds, this->isOn);
    }
}