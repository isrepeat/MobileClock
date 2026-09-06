#include <XamlRuntime/RenderEngine.h>

#include "UI/PageTransition.h"
#include "AnimationRenderers.h"

#include <algorithm>
#include <cmath>

namespace mobileclock::renderer::_details {
    bool ValidDuration(const int& duration) {
        return duration >= 0;
    }

    // Расширяет стандартный Wave для кнопки, не заменяя её базовый рендер.
    bool RenderWaveOutline(const xaml::Element& element, xaml::RenderContext<mobileclock::resources::effects::WaveAnimation>& context) {
        if (element.Type() != xaml::ElementType::button) {
            return false;
        }

        // Сначала рисуем chrome, текст, дочерние элементы и штатный Wave shader.
        context.RenderDefaultElement();
        mobileclock::resources::effects::RenderWave(element, context);

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
    bool ConfigurePageTransition(xaml::AnimationContext<mobileclock::resources::effects::ContainerAnimation>& context) {
        if (context.Trigger() != xaml::AnimationTrigger::show && context.Trigger() != xaml::AnimationTrigger::hide) {
            return false;
        }
        const auto* data = context.Parameters().TryGet<ui::PageTransitionData>();
        const bool forward = data == nullptr || data->direction == ui::NavigationDirection::forward;
        const bool show = context.Trigger() == xaml::AnimationTrigger::show;
        const float direction = forward ? 1.0f : -1.0f;
        const float width = context.Target().Bounds().width;
        if (context.IsStartingFromHidden()) {
            context.Transform().offsetX = direction * width;
            context.Transform().opacity = 0.0f;
        }
        context.AnimateTransform(&xaml::VisualTransform::offsetX, show ? 0.0f : -direction * width, std::chrono::milliseconds(240));
        context.AnimateTransform(&xaml::VisualTransform::opacity, show ? 1.0f : 0.0f, std::chrono::milliseconds(180));
        return true;
    }

    // Асимметричная анимация, выбранная в Storyboard для Show/Hide.
    bool ConfigureSettingsReveal(xaml::AnimationContext<mobileclock::resources::effects::ContainerAnimation>& context) {
        if (context.Trigger() != xaml::AnimationTrigger::show && context.Trigger() != xaml::AnimationTrigger::hide) {
            return false;
        }
        const auto* data = context.Parameters().TryGet<ui::PageTransitionData>();
        if (data == nullptr || data->to != "settings") {
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
}

namespace mobileclock::renderer {
    void RegisterAnimationRenderers(xaml::RendererRegistry& renderers) {
        // Сохраняет стандартный Wave и добавляет поверх него пульсирующую обводку.
        renderers.Register<mobileclock::resources::effects::WaveAnimation>("rendererWaveOutline", _details::RenderWaveOutline);
    }

    void RegisterAnimations(xaml::AnimationRegistry& animations) {
        animations.Register<mobileclock::resources::effects::ContainerAnimation>("animationPageTransition", {}, _details::ConfigurePageTransition);
        animations.Register<mobileclock::resources::effects::ContainerAnimation>("animationSettingsReveal", {
            xaml::Option("duration", &mobileclock::resources::effects::ContainerAnimation::duration, _details::ValidDuration),
        }, _details::ConfigureSettingsReveal);
    }
}