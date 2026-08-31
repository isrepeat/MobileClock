#include "Controls/ToggleSwitchControl.h"

#include "Renderer/ControlRenderer.h"

namespace mobileclock::ui {
    ToggleSwitchControl::ToggleSwitchControl(xaml::Element& element)
        : element(element) {
    }

    //
    // IControl
    //
    bool ToggleSwitchControl::HitTest(float x, float y) const {
        const xaml::Rect bounds = this->element.Bounds();
        return x >= bounds.x && x <= bounds.x + bounds.width
            && y >= bounds.y && y <= bounds.y + bounds.height;
    }

    bool ToggleSwitchControl::HandleTap(float x, float y) {
        if (!this->HitTest(x, y)) {
            return false;
        }
        this->element.SetIsOn(!this->element.IsOn());
        return true;
    }

    void ToggleSwitchControl::Render(mobileclock::renderer::ControlRenderer& renderer) const {
        renderer.DrawToggleSwitch(this->element.Bounds(), this->element.IsOn());
    }

    xaml::Element& ToggleSwitchControl::ElementModel() {
        return this->element;
    }
}