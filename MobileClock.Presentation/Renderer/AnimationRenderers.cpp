#include "AnimationRenderers.h"

#include <XamlRuntime/RenderEngine.h>

#include "../Core/PageTransition.h"

#include <algorithm>
#include <cmath>

namespace mobileclock::presentation::renderer::_details {
    bool ValidDuration(const int& duration) {
        return duration >= 0;
    }

    bool ValidEasing(const std::string& easing) {
        return easing == "Linear" || easing == "CubicOut";
    }

    bool ValidDirection(const std::string& direction) {
        return direction.empty() || direction == "forward" || direction == "backward";
    }

    bool ValidPhase(const std::string& phase) {
        return phase.empty() || phase == "enter" || phase == "exit";
    }

    // Расширяет стандартный Wave для кнопки, не заменяя её базовый рендер.
    bool RenderWaveOutline(const xaml::Element& element, xaml::RenderContext<mobileclock::presentation::effects::WaveAnimation>& context) {
        if (element.Type() != xaml::ElementType::button) {
            return false;
        }

        // Сначала рисуем chrome, текст, дочерние элементы и штатный Wave shader.
        context.RenderDefaultElement();
        mobileclock::presentation::effects::RenderWave(element, context);

        const float progress = context.State().progress;
        if (progress < 0.0f || context.State().opacity <= 0.0f) {
            return true;
        }

        // Обводка затухает и сужается синхронно с распространением Wave.
        const float pulse = 1.0f - std::min(progress, 1.0f);
        const xaml::attr::Color foreground = element.Foreground();
        const xaml::attr::Color color{
            foreground.red,
            foreground.green,
            foreground.blue,
            foreground.alpha * context.State().opacity * pulse * context.Opacity(),
        };
        const float thickness = 1.0f + pulse * 3.0f;
        context.Backend().DrawRoundedRectOutline(
            context.Bounds(),
            color,
            element.CornerRadius(),
            thickness);
        return true;
    }
    bool ConfigurePageTransition(xaml::AnimationContext<mobileclock::presentation::effects::PageTransitionAnimation>& context) {
        const bool visualState = context.Trigger() == xaml::AnimationTrigger::visualState;
        if (!visualState && context.Trigger() != xaml::AnimationTrigger::show
            && context.Trigger() != xaml::AnimationTrigger::hide) {
            return false;
        }
        const auto* pageTransitionData = context.Parameters().TryGet<mobileclock::presentation::core::PageTransitionData>();
        const bool forward = visualState
            ? context.State().direction == "forward"
            : pageTransitionData == nullptr
            || pageTransitionData->direction == mobileclock::presentation::core::NavigationDirection::forward;
        const bool show = visualState
            ? context.State().phase == "enter"
            : context.Trigger() == xaml::AnimationTrigger::show;
        const float direction = forward ? 1.0f : -1.0f;
        const auto& settings = context.State();
        const float distance = context.Target().Bounds().width * settings.distance;
        if ((visualState && show) || context.IsStartingFromHidden()) {
            context.Transform().offsetX = direction * distance;
        }
        context.AnimateTransform(&xaml::VisualTransform::offsetX, show ? 0.0f : -direction * distance,
            std::chrono::milliseconds(settings.duration),
            settings.easing == "Linear" ? xaml::Easing::linear : xaml::Easing::cubicOut);
        return true;
    }

    // Асимметричная анимация, выбранная в Storyboard для Show/Hide.
    bool ConfigureSettingsReveal(xaml::AnimationContext<mobileclock::presentation::effects::ContainerAnimation>& context) {
        if (context.Trigger() != xaml::AnimationTrigger::show && context.Trigger() != xaml::AnimationTrigger::hide) {
            return false;
        }
        const auto* pageTransitionData = context.Parameters().TryGet<mobileclock::presentation::core::PageTransitionData>();
        if (pageTransitionData == nullptr || pageTransitionData->to != "settings") {
            return false;
        }
        const bool show = context.Trigger() == xaml::AnimationTrigger::show;
        if (context.IsStartingFromHidden()) {
            context.Transform().offsetY = 32.0f;
            context.Transform().opacity = 0.0f;
        }
        context.AnimateTransform(&xaml::VisualTransform::offsetX, show ? 0.0f : -48.0f, std::chrono::milliseconds(context.State().duration));
        context.AnimateTransform(&xaml::VisualTransform::offsetY, 0.0f, std::chrono::milliseconds(context.State().duration));
        context.AnimateTransform(&xaml::VisualTransform::opacity, show ? 1.0f : 0.0f, std::chrono::milliseconds(context.State().duration));
        return true;
    }
} // namespace _details

namespace mobileclock::presentation::renderer {
    void RegisterAnimationRenderers(xaml::RendererRegistry& rendererRegistry) {
        // Сохраняет стандартный Wave и добавляет поверх него пульсирующую обводку.
        rendererRegistry.Register<mobileclock::presentation::effects::WaveAnimation>("rendererWaveOutline", _details::RenderWaveOutline);
    }

    void RegisterAnimations(xaml::AnimationRegistry& animationRegistry) {
        animationRegistry.Register<mobileclock::presentation::effects::PageTransitionAnimation>("animationPageTransition", {
            xaml::Option("duration", &mobileclock::presentation::effects::PageTransitionAnimation::duration, _details::ValidDuration),
            xaml::Option("distance", &mobileclock::presentation::effects::PageTransitionAnimation::distance),
            xaml::Option("easing", &mobileclock::presentation::effects::PageTransitionAnimation::easing, _details::ValidEasing),
            xaml::Option("direction", &mobileclock::presentation::effects::PageTransitionAnimation::direction, _details::ValidDirection),
            xaml::Option("phase", &mobileclock::presentation::effects::PageTransitionAnimation::phase, _details::ValidPhase),
        }, _details::ConfigurePageTransition);
        animationRegistry.Register<mobileclock::presentation::effects::ContainerAnimation>("animationSettingsReveal", {
            xaml::Option("duration", &mobileclock::presentation::effects::ContainerAnimation::duration, _details::ValidDuration),
        }, _details::ConfigureSettingsReveal);
    }
}