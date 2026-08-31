#include "Controls/PageControl.h"

namespace mobileclock::ui {
    PageControl::PageControl(xaml::Element& element)
        : element(element) {
    }

    //
    // IControl
    //
    bool PageControl::HitTest(float x, float y) const {
        return false;
    }

    bool PageControl::HandleTap(float x, float y) {
        return false;
    }

    void PageControl::Render(mobileclock::renderer::ControlRenderer& renderer) const {
        static_cast<void>(renderer);
    }

    xaml::Element& PageControl::ElementModel() {
        return this->element;
    }
}