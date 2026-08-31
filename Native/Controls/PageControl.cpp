#include "Controls/PageControl.h"

#include "Renderer/ControlRenderer.h"

namespace mobileclock::ui {
    PageControl::PageControl(Element& element)
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
        renderer.DrawRoundedRect(this->element.Bounds(), this->element.Background(), 0.0f);
    }

    Element& PageControl::ElementModel() {
        return this->element;
    }
}