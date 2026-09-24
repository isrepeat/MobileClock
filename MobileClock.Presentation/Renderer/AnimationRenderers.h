#pragma once
#include "../Effects/Effects.h"

namespace xaml {
    class RendererRegistry;
    class AnimationRegistry;
}

namespace mobileclock::presentation::renderer {
    void RegisterAnimationRenderers(xaml::RendererRegistry& rendererRegistry);
    void RegisterAnimations(xaml::AnimationRegistry& animationRegistry);
}