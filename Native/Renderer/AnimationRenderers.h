#pragma once

namespace xaml {
    class RendererRegistry;
}

namespace mobileclock::renderer {
    void RegisterAnimationRenderers(xaml::RendererRegistry& renderers);
}