#pragma once

#include "Interfaces/IControl.h"
#include "XamlRuntime/XamlLayout.h"

namespace mobileclock::ui {
    class ToggleSwitchControl final : public IControl {
    public:
        explicit ToggleSwitchControl(const Element& element);

        bool HitTest(float x, float y) const override;
        bool HandleTap(float x, float y) override;
        void Render(mobileclock::renderer::ControlRenderer& renderer) const override;

    private:
        Rect bounds;
        bool isOn;
    };
}