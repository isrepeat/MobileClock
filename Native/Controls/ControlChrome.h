#pragma once

namespace mobileclock::renderer {
    class ControlRenderer;
}

namespace xaml {
    class Element;
}

namespace mobileclock::ui {
    void RenderControlChrome(const xaml::Element& element,
        mobileclock::renderer::ControlRenderer& renderer);
}