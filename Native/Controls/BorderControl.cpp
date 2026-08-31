#include "Controls/BorderControl.h"

namespace mobileclock::ui {
    BorderControl::BorderControl(xaml::Element& element)
        : element(element) {
    }

    //
    // IControl
    //
    bool BorderControl::HitTest(float x, float y) const {
        const xaml::Rect bounds = this->element.Bounds();
        return x >= bounds.x && x <= bounds.x + bounds.width
            && y >= bounds.y && y <= bounds.y + bounds.height;
    }

    bool BorderControl::HandleTap(float x, float y) {
        return this->HitTest(x, y);
    }

    void BorderControl::Render(mobileclock::renderer::ControlRenderer& renderer) const {
        static_cast<void>(renderer);
    }

    xaml::Element& BorderControl::ElementModel() {
        return this->element;
    }
}