#pragma once

#include <ESRenderer/OpenGlRenderer.h>

#include <vector>

namespace mobileclock::renderer {
    es_renderer::OpenGlRenderer::ShaderProgramSources CreateShaderPrograms(
        const std::vector<unsigned char>& rippleVertex,
        const std::vector<unsigned char>& rippleFragment);
}