#include "Registrations.h"

#include "../Renderer/AnimationRenderers.h"
#include "../Effects/Effects.h"
#include "../Effects/Shaders.h"

#include <utility>

namespace mobileclock::presentation::core {
    //
    // API
    //
    void RegisterAnimations(xaml::AnimationRegistry& registry) {
        xaml::AnimationRegistry effects = mobileclock::presentation::effects::CreateAnimations();
        mobileclock::presentation::renderer::RegisterAnimations(effects);
        registry = std::move(effects);
    }

    void RegisterRenderers(xaml::RendererRegistry& registry) {
        registry = mobileclock::presentation::effects::CreateRenderers();
        mobileclock::presentation::renderer::RegisterAnimationRenderers(registry);
    }

    es_renderer::OpenGlRenderer::ShaderProgramSources CreateShaderPrograms() {
        return mobileclock::presentation::effects::CreateShaderPrograms();
    }
}