#include "Controls/BorderControl.h"

#include "Renderer/ControlRenderer.h"

#include <algorithm>

namespace mobileclock::ui {
    BorderControl::BorderControl(Element& element)
        : element(element) {
    }

    //
    // IControl
    //
    bool BorderControl::HitTest(float x, float y) const {
        const Rect bounds = this->element.Bounds();
        return x >= bounds.x && x <= bounds.x + bounds.width
            && y >= bounds.y && y <= bounds.y + bounds.height;
    }

    bool BorderControl::HandleTap(float x, float y) {
        return this->HitTest(x, y);
    }

    void BorderControl::Render(mobileclock::renderer::ControlRenderer& renderer) const {
        const attr::Thickness borderThickness = this->element.BorderThickness();
        const bool hasBorder = borderThickness.left > 0.0f || borderThickness.right > 0.0f
            || borderThickness.top > 0.0f || borderThickness.bottom > 0.0f;
        if (!hasBorder) {
            renderer.DrawRoundedRect(this->element.Bounds(), this->element.Background(), this->element.CornerRadius());
            return;
        }
        const Rect bounds = this->element.Bounds();
        renderer.DrawRoundedRect(bounds, this->element.BorderColor(), this->element.CornerRadius());
        const Rect innerBounds{
            bounds.x + borderThickness.left,
            bounds.y + borderThickness.top,
            bounds.width - borderThickness.left - borderThickness.right,
            bounds.height - borderThickness.top - borderThickness.bottom,
        };
        const float innerRadius = this->element.CornerRadius() - std::max({
            borderThickness.left,
            borderThickness.right,
            borderThickness.top,
            borderThickness.bottom,
        });
        renderer.DrawRoundedRect(innerBounds, this->element.Background(), std::max(0.0f, innerRadius));
    }

    Element& BorderControl::ElementModel() {
        return this->element;
    }
}