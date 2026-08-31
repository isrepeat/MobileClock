#pragma once

#include "Interfaces/IControl.h"
#include "XamlRuntime/XamlLayout.h"

namespace mobileclock::ui {
    class ButtonControl final : public IControl {
    public:
        explicit ButtonControl(const Element& element);

        bool HitTest(float x, float y) const override;
        bool HandleTap(float x, float y) override;
        void Render(mobileclock::renderer::ControlRenderer& renderer) const override;

    private:
        Rect bounds;
        attr::Color foreground;
        std::string text;
        bool isBlue = false;
    };
}