#include <XamlRuntime/RenderEngine.h>

#include "AnimationRenderers.h"

#include <algorithm>
#include <cmath>

namespace mobileclock::renderer::_details {
    // Расширяет стандартный Wave для кнопки, не заменяя её базовый рендер.
    bool RenderWaveOutline(const xaml::Element& element, xaml::RenderContext& context) {
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
}

namespace mobileclock::renderer {
    void RegisterAnimationRenderers(xaml::RendererRegistry& renderers) {
        // Сохраняет стандартный Wave и добавляет поверх него пульсирующую обводку.
        renderers.Register("wave-outline", _details::RenderWaveOutline);
    }
}