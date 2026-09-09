#include "AnimationShaders.h"

#include "MobileClock.Presentation/Effects/Shaders.h"

namespace mobileclock::renderer {
    es_renderer::OpenGlRenderer::ShaderProgramSources CreateShaderPrograms() {
        return mobileclock::resources::effects::CreateShaderPrograms();
    }
}