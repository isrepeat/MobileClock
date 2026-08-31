#pragma once

namespace mobileclock::renderer {
    class ControlRenderer;
}

namespace xaml {
    class Element;
}

namespace mobileclock::ui {
    class IControl {
    public:
        virtual ~IControl() = default;

        virtual bool HitTest(float x, float y) const = 0;
        virtual bool HandleTap(float x, float y) = 0;
        virtual void Render(mobileclock::renderer::ControlRenderer& renderer) const = 0;
        virtual xaml::Element& ElementModel() = 0;
    };
}