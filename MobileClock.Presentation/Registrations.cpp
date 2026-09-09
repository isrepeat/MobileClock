#include "Registrations.h"

#include "AnimationRenderers.h"
#include "Effects/Effects.h"
#include "Effects/Shaders.h"

#include <utility>

namespace mobileclock::presentation {
    //
    // API
    //
    void RegisterAnimations(xaml::AnimationRegistry& registry) {
        xaml::AnimationRegistry effects = mobileclock::resources::effects::CreateAnimations();
        mobileclock::renderer::RegisterAnimations(effects);
        registry = std::move(effects);
    }

    void RegisterRenderers(xaml::RendererRegistry& registry) {
        registry = mobileclock::resources::effects::CreateRenderers();
        mobileclock::renderer::RegisterAnimationRenderers(registry);
    }

    es_renderer::OpenGlRenderer::ShaderProgramSources CreateShaderPrograms() {
        return mobileclock::resources::effects::CreateShaderPrograms();
    }
}