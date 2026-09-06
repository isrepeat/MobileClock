#include "Effects.h"

#include <algorithm>
#include <cmath>

namespace mobileclock::resources::effects::_details {
    using namespace xaml;

    bool NonNegativeDuration(const int& value) {
        return value >= 0;
    }

    bool NonNegative(const float& value) {
        return value >= 0.0f;
    }

    bool Positive(const float& value) {
        return value > 0.0f;
    }

    bool UnitInterval(const float& value) {
        return value >= 0.0f && value <= 1.0f;
    }

    bool WaveFrom(const std::string& value) {
        if (value != "Current") {
            xaml::_details::ParseStateFloat(value);
        }
        return true;
    }

    bool WaveEasing(const std::string& value) {
        return value == "Linear" || value == "CubicOut";
    }

    bool ConfigureWave(AnimationContext<WaveAnimation>& context) {
        auto& wave = context.State();
        if (wave.from != "Current") {
            wave.progress = xaml::_details::ParseStateFloat(wave.from);
        }
        context.Animate(&WaveAnimation::progress, wave.to,
            std::chrono::milliseconds(wave.duration),
            wave.easing == "Linear" ? Easing::linear : Easing::cubicOut);
        return true;
    }

    bool ConfigureFade(AnimationContext<ContainerAnimation>& context) {
        if (context.Trigger() != AnimationTrigger::show && context.Trigger() != AnimationTrigger::hide) {
            return false;
        }
        auto& state = context.Transform();
        if (context.IsStartingFromHidden()) {
            state.opacity = 0.0f;
        }
        context.AnimateTransform(&VisualTransform::opacity, context.Trigger() == AnimationTrigger::show ? 1.0f : 0.0f,
            std::chrono::milliseconds(context.State().duration));
        return true;
    }

    bool ConfigureSlideFade(AnimationContext<ContainerAnimation>& context) {
        if (!ConfigureFade(context)) {
            return false;
        }
        auto& state = context.Transform();
        if (context.IsStartingFromHidden()) {
            state.offsetY = context.State().distance;
        }
        context.AnimateTransform(&VisualTransform::offsetY,
            context.Trigger() == AnimationTrigger::show ? 0.0f : context.State().distance,
            std::chrono::milliseconds(context.State().duration));
        return true;
    }

    bool ConfigureGlow(AnimationContext<Glow>& context) {
        const auto& glow = context.State();
        context.Animate(&Glow::intensity, glow.targetIntensity, std::chrono::milliseconds(glow.duration));
        return true;
    }
    bool ConfigureWaveOpacity(AnimationContext<WaveAnimation>& context) {
        if (context.State().opacityFrom != "Current") {
            context.State().opacity = xaml::_details::ParseStateFloat(context.State().opacityFrom);
        }
        context.Animate(&WaveAnimation::opacity, context.State().targetOpacity,
            std::chrono::milliseconds(context.State().duration),
            context.State().easing == "Linear" ? Easing::linear : Easing::cubicOut);
        return true;
    }

    bool RenderGlow(const Element& element, RenderContext<Glow>& context) {
        context.RenderDefault();
        const float intensity = context.State().intensity;
        if (intensity <= 0.0f) {
            return true;
        }
        auto color = element.Foreground();
        color.alpha *= intensity * context.Opacity();
        context.Backend().DrawRoundedRectOutline(
            context.Bounds(), color, element.CornerRadius(), 3.0f);
        return true;
    }


    void RenderButtonWave(
        const Element& element,
        IRenderBackend& backend,
        Rect bounds,
        float opacity) {
        const auto& wave = element.State<WaveAnimation>();
        const float progress = wave.progress;
        if (progress < 0.0f || wave.opacity <= 0.0f) {
            return;
        }

        const float fade = std::pow(wave.opacity, wave.fadeExponent);
        const attr::Color color{
            element.Foreground().red,
            element.Foreground().green,
            element.Foreground().blue,
            element.Foreground().alpha * fade * wave.intensity,
        };
        attr::Color waveColor = color;
        waveColor.alpha *= opacity;
        backend.DrawShader(
            "button-wave",
            bounds,
            {
                {"cornerRadius", {element.CornerRadius()}, 1},
                {"progress", {std::min(progress, 1.0f)}, 1},
                {"spread", {wave.spread}, 1},
                {"rippleColor", {
                    waveColor.red,
                    waveColor.green,
                    waveColor.blue,
                    waveColor.alpha,
                }, 4},
            });
    }

    bool RenderWave(const Element& element, RenderContext<WaveAnimation>& context) {
        context.RenderDefault();
        RenderButtonWave(element, context.Backend(), context.Bounds(), context.Opacity());
        return true;
    }

}

namespace mobileclock::resources::effects {
    using namespace xaml;

    void RenderWave(const Element& element, RenderContext<WaveAnimation>& context) {
        _details::RenderButtonWave(element, context.Backend(), context.Bounds(), context.Opacity());
    }

    StateRegistry CreateStates() {
        StateRegistry states;
        states.Register<ContainerAnimation>();
        states.Register<PageTransitionAnimation>();
        states.Register<WaveAnimation>();
        states.Register<Glow>();
        return states;
    }

    AnimationRegistry CreateAnimations() {
        AnimationRegistry animations(CreateStates());
        RegisterAnimations(animations);
        return animations;
    }

    RendererRegistry CreateRenderers() {
        RendererRegistry renderers(CreateStates());
        RegisterRenderers(renderers);
        return renderers;
    }

    void RegisterAnimations(AnimationRegistry& animations) {
        animations.Register<ContainerAnimation>("animationFade", {
            Option("duration", &ContainerAnimation::duration, _details::NonNegativeDuration),
        }, _details::ConfigureFade);
        animations.Register<ContainerAnimation>("animationSlideFade", {
            Option("duration", &ContainerAnimation::duration, _details::NonNegativeDuration),
            Option("distance", &ContainerAnimation::distance),
        }, _details::ConfigureSlideFade);
        for (const char* name : {"animationSoftPulse", "animationRippleWave"}) {
            animations.Register<WaveAnimation>(name, {
                Option("from", &WaveAnimation::from, _details::WaveFrom),
                Option("to", &WaveAnimation::to),
                Option("duration", &WaveAnimation::duration, _details::NonNegativeDuration),
                Option("easing", &WaveAnimation::easing, _details::WaveEasing),
                Option("intensity", &WaveAnimation::intensity, _details::NonNegative),
                Option("spread", &WaveAnimation::spread, _details::Positive),
                Option("fadeExponent", &WaveAnimation::fadeExponent, _details::Positive),
            }, _details::ConfigureWave);
        }
        animations.Register<Glow>("animationGlow", {
            Option("intensity", &Glow::targetIntensity, _details::UnitInterval),
            Option("duration", &Glow::duration, _details::NonNegativeDuration),
        }, _details::ConfigureGlow);
        animations.Register<WaveAnimation>("animationWaveOpacity", {
            Option("from", &WaveAnimation::opacityFrom, _details::WaveFrom),
            Option("to", &WaveAnimation::targetOpacity, _details::UnitInterval),
            Option("duration", &WaveAnimation::duration, _details::NonNegativeDuration),
            Option("easing", &WaveAnimation::easing, _details::WaveEasing),
        }, _details::ConfigureWaveOpacity);
    }

    void RegisterRenderers(RendererRegistry& renderers) {
        renderers.Register<Glow>("rendererGlow", _details::RenderGlow);
        renderers.Register<WaveAnimation>("rendererWave", _details::RenderWave);
    }
}