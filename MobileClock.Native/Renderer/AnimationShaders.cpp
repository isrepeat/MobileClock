#include "Resources/Effects/Shaders.h"
#include "AnimationShaders.h"

namespace mobileclock::renderer {
    es_renderer::OpenGlRenderer::ShaderProgramSources CreateShaderPrograms() {
        return mobileclock::resources::effects::CreateShaderPrograms();
    }
}