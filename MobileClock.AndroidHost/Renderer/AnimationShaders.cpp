#include "AnimationShaders.h"

#include "MobileClock.Presentation/Effects/Shaders.h"

namespace mobileclock::android_host::renderer {
    es_renderer::OpenGlRenderer::ShaderProgramSources CreateShaderPrograms() {
        return mobileclock::presentation::effects::CreateShaderPrograms();
    }
}