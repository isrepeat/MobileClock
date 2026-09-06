#pragma once

#include <XamlRuntime/RenderEngine.h>

namespace mobileclock::resources::effects {
    struct ContainerAnimation {
        int duration = 180;
        float distance = 24.0f;
    };

    struct WaveAnimation {
        float progress = 1.0f;
        float opacity = 0.0f;
        float targetOpacity = 1.0f;
        std::string opacityFrom = "Current";
        std::string from = "0";
        float to = 1.0f;
        int duration = 650;
        std::string easing = "CubicOut";
        float intensity = 0.45f;
        float spread = 0.28f;
        float fadeExponent = 2.0f;
    };

    struct Glow {
        float intensity = 0.0f;
        float targetIntensity = 0.8f;
        int duration = 300;
    };

    void RenderWave(const xaml::Element& element, xaml::RenderContext<WaveAnimation>& context);
    xaml::StateRegistry CreateStates();
    xaml::AnimationRegistry CreateAnimations();
    xaml::RendererRegistry CreateRenderers();
    void RegisterAnimations(xaml::AnimationRegistry& animations);
    void RegisterRenderers(xaml::RendererRegistry& renderers);
}