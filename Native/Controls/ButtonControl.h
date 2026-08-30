#pragma once

#include "Interfaces/IControl.h"
#include "XamlRuntime/XamlLayout.h"

namespace mobileclock::ui {
    class ButtonControl final : public IControl {
    public:
        explicit ButtonControl(const Element& element);

        bool handleTap(float x, float y) override;
        void render(mobileclock::renderer::ControlRenderer& renderer) const override;

    private:
        Rect _bounds;
        Color _foreground;
        std::string _text;
        bool _isBlue = false;
    };
}