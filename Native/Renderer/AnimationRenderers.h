#pragma once

namespace xaml {
    class RendererRegistry;
    class AnimationRegistry;
}

namespace mobileclock::renderer {
    void RegisterAnimationRenderers(xaml::RendererRegistry& renderers);
    void RegisterAnimations(xaml::AnimationRegistry& animations);
}