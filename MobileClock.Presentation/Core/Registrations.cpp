#include "Registrations.h"

#include "../Renderer/AnimationRenderers.h"
#include "../Effects/Effects.h"
#include "../Effects/Shaders.h"

#include <utility>

namespace mobileclock::presentation::core {
    //
    // API
    //
    void RegisterAnimations(xaml::AnimationRegistry& animationRegistry) {
        xaml::AnimationRegistry effectsAnimationRegistry = mobileclock::presentation::effects::CreateAnimations();
        mobileclock::presentation::renderer::RegisterAnimations(effectsAnimationRegistry);
        animationRegistry = std::move(effectsAnimationRegistry);
    }

    void RegisterRenderers(xaml::RendererRegistry& rendererRegistry) {
        rendererRegistry = mobileclock::presentation::effects::CreateRenderers();
        mobileclock::presentation::renderer::RegisterAnimationRenderers(rendererRegistry);
    }

    es_renderer::OpenGlRenderer::ShaderProgramSources CreateShaderPrograms() {
        return mobileclock::presentation::effects::CreateShaderPrograms();
    }
}