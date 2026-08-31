#pragma once

#include "Interfaces/IControl.h"
#include "XamlRuntime/XamlLayout.h"

namespace mobileclock::ui {
    class BorderControl final : public IControl {
    public:
        explicit BorderControl(const Element& element);

        bool HitTest(float x, float y) const override;
        bool HandleTap(float x, float y) override;
        void Render(mobileclock::renderer::ControlRenderer& renderer) const override;

    private:
        Rect bounds;
        attr::Color background;
        attr::Color borderColor;
        attr::Thickness borderThickness;
        float cornerRadius;
    };
}