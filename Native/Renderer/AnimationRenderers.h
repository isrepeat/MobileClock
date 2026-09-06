#pragma once

#include "Resources/XamlHost/Effects.h"

namespace xaml {
    class RendererRegistry;
    class AnimationRegistry;
}

namespace mobileclock::renderer {
    void RegisterAnimationRenderers(xaml::RendererRegistry& renderers);
    void RegisterAnimations(xaml::AnimationRegistry& animations);
}