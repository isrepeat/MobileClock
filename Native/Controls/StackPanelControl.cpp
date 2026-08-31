#include "Controls/StackPanelControl.h"

namespace mobileclock::ui {
    StackPanelControl::StackPanelControl(xaml::Element& element)
        : element(element) {
    }

    //
    // IControl
    //
    bool StackPanelControl::HitTest(float x, float y) const {
        return false;
    }

    bool StackPanelControl::HandleTap(float x, float y) {
        return false;
    }

    void StackPanelControl::Render(mobileclock::renderer::ControlRenderer& renderer) const {
        static_cast<void>(renderer);
    }

    xaml::Element& StackPanelControl::ElementModel() {
        return this->element;
    }
}