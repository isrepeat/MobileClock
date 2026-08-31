#pragma once

#include "Interfaces/IControl.h"
#include "XamlRuntime/XamlLayout.h"

namespace mobileclock::ui {
    class PageControl final : public IControl {
    public:
        explicit PageControl(Element& element);

        //
        // IControl
        //
        bool HitTest(float x, float y) const override;
        bool HandleTap(float x, float y) override;
        void Render(mobileclock::renderer::ControlRenderer& renderer) const override;
        Element& ElementModel() override;

    private:
        Element& element;
    };
}