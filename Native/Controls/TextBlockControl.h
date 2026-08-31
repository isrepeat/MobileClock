#pragma once

#include "Interfaces/IControl.h"
#include "XamlRuntime/XamlLayout.h"

namespace mobileclock::ui {
    class TextBlockControl final : public IControl {
    public:
        explicit TextBlockControl(Element& element);

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