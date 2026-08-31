#include "Controls/TextBlockControl.h"

#include "Renderer/ControlRenderer.h"

namespace mobileclock::ui {
    TextBlockControl::TextBlockControl(Element& element)
        : element(element) {
    }

    //
    // IControl
    //
    bool TextBlockControl::HitTest(float x, float y) const {
        return false;
    }

    bool TextBlockControl::HandleTap(float x, float y) {
        return false;
    }

    void TextBlockControl::Render(mobileclock::renderer::ControlRenderer& renderer) const {
        renderer.DrawText(this->element.Bounds(), this->element.Text(), this->element.Foreground());
    }

    Element& TextBlockControl::ElementModel() {
        return this->element;
    }
}