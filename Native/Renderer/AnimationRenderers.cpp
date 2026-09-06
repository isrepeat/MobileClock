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
    bool RenderWaveOutline(const xaml::Element& element, xaml::RenderContext<xaml::WaveAnimation>& context) {
        if (element.Type() != xaml::ElementType::button) {
            return false;
        }

        // Сначала рисуем chrome, текст, дочерние элементы и штатный Wave shader.
        context.RenderDefaultElement();

        const float progress = element.WaveProgress();
        if (progress < 0.0f || element.WaveOpacity() <= 0.0f) {
            return true;
        }

        // Обводка затухает и сужается синхронно с распространением Wave.
        const float pulse = 1.0f - std::min(progress, 1.0f);
        const xaml::attr::Color foreground = element.Foreground();
        const xaml::attr::Color color{
            foreground.red,
            foreground.green,
            foreground.blue,
            foreground.alpha * element.WaveOpacity() * pulse * context.Opacity(),
        };
        const float thickness = 1.0f + pulse * 3.0f;
        context.Backend().DrawRoundedRectOutline(
            context.Bounds(),
            color,
            element.CornerRadius(),
            thickness);
        return true;
    }
    bool ConfigurePageTransition(xaml::AnimationContext<xaml::ContainerAnimation>& context) {
        if (context.Trigger() != xaml::AnimationTrigger::show && context.Trigger() != xaml::AnimationTrigger::hide) {
            return false;
        }
        const auto* data = context.Parameters().TryGet<ui::PageTransitionData>();
        const bool forward = data == nullptr || data->direction == ui::NavigationDirection::forward;
        const bool show = context.Trigger() == xaml::AnimationTrigger::show;
        const float direction = forward ? 1.0f : -1.0f;
        const float width = context.Target().Bounds().width;
        if (context.IsStartingFromHidden()) {
            context.State().offsetX = direction * width;
            context.State().opacity = 0.0f;
        }
        context.Animate(&xaml::ContainerAnimation::offsetX, show ? 0.0f : -direction * width, std::chrono::milliseconds(240));
        context.Animate(&xaml::ContainerAnimation::opacity, show ? 1.0f : 0.0f, std::chrono::milliseconds(180));
        return true;
    }

    // Asymmetric custom animation selected inside Show/Hide storyboards.
    bool ConfigureSettingsReveal(xaml::AnimationContext<xaml::ContainerAnimation>& context) {
        if (context.Trigger() != xaml::AnimationTrigger::show && context.Trigger() != xaml::AnimationTrigger::hide) {
            return false;
        }
        const auto* data = context.Parameters().TryGet<ui::PageTransitionData>();
        if (data == nullptr || data->to != "settings") {
            return false;
        }
        const bool show = context.Trigger() == xaml::AnimationTrigger::show;
        if (context.IsStartingFromHidden()) {
            context.State().offsetY = 32.0f;
            context.State().opacity = 0.0f;
        }
        context.Animate(&xaml::ContainerAnimation::offsetX, show ? 0.0f : -48.0f, std::chrono::milliseconds(context.State().duration));
        context.Animate(&xaml::ContainerAnimation::offsetY, 0.0f, std::chrono::milliseconds(context.State().duration));
        context.Animate(&xaml::ContainerAnimation::opacity, show ? 1.0f : 0.0f, std::chrono::milliseconds(context.State().duration));
        return true;
    }
}

namespace mobileclock::renderer {
    void RegisterAnimationRenderers(xaml::RendererRegistry& renderers) {
        // Сохраняет стандартный Wave и добавляет поверх него пульсирующую обводку.
        renderers.Register<xaml::WaveAnimation>("rendererWaveOutline", _details::RenderWaveOutline);
    }

    void RegisterAnimations(xaml::AnimationRegistry& animations) {
        animations.Register<xaml::ContainerAnimation>("animationPageTransition", {}, _details::ConfigurePageTransition);
        animations.Register<xaml::ContainerAnimation>("animationSettingsReveal", {
            xaml::Option("duration", &xaml::ContainerAnimation::duration, _details::ValidDuration),
        }, _details::ConfigureSettingsReveal);
    }
}