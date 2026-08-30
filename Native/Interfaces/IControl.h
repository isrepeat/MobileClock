#pragma once

namespace mobileclock::renderer {
    class ControlRenderer;
}

namespace mobileclock::ui {
    class IControl {
    public:
        virtual ~IControl() = default;

        virtual bool handleTap(float x, float y) = 0;
        virtual void render(mobileclock::renderer::ControlRenderer& renderer) const = 0;
    };
}