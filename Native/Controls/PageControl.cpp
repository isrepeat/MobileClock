#include "Controls/PageControl.h"

#include "Renderer/ControlRenderer.h"

namespace mobileclock::ui {
    PageControl::PageControl(const Element& element)
        : bounds(element.Bounds())
        , background(element.Background()) {
    }

    bool PageControl::HandleTap(float x, float y) {
        return false;
    }

    void PageControl::Render(mobileclock::renderer::ControlRenderer& renderer) const {
        renderer.DrawRoundedRect(this->bounds, this->background, 0.0f);
    }
}