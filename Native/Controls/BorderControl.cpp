#include "Controls/BorderControl.h"

#include "Renderer/ControlRenderer.h"

#include <algorithm>

namespace mobileclock::ui {
    BorderControl::BorderControl(const Element& element)
        : bounds(element.Bounds())
        , background(element.Background())
        , borderColor(element.BorderColor())
        , borderThickness(element.BorderThickness())
        , cornerRadius(element.CornerRadius()) {
    }

    bool BorderControl::HandleTap(float x, float y) {
        return x >= this->bounds.x && x <= this->bounds.x + this->bounds.width
            && y >= this->bounds.y && y <= this->bounds.y + this->bounds.height;
    }

    void BorderControl::Render(mobileclock::renderer::ControlRenderer& renderer) const {
        const bool hasBorder = this->borderThickness.left > 0.0f || this->borderThickness.right > 0.0f
            || this->borderThickness.top > 0.0f || this->borderThickness.bottom > 0.0f;
        if (!hasBorder) {
            renderer.DrawRoundedRect(this->bounds, this->background, this->cornerRadius);
            return;
        }
        renderer.DrawRoundedRect(this->bounds, this->borderColor, this->cornerRadius);
        const Rect innerBounds{
            this->bounds.x + this->borderThickness.left,
            this->bounds.y + this->borderThickness.top,
            this->bounds.width - this->borderThickness.left - this->borderThickness.right,
            this->bounds.height - this->borderThickness.top - this->borderThickness.bottom,
        };
        const float innerRadius = this->cornerRadius - std::max({
            this->borderThickness.left,
            this->borderThickness.right,
            this->borderThickness.top,
            this->borderThickness.bottom,
        });
        renderer.DrawRoundedRect(innerBounds, this->background, std::max(0.0f, innerRadius));
    }
}