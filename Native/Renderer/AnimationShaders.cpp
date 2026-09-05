#include "AnimationShaders.h"

namespace mobileclock::renderer {
    es_renderer::OpenGlRenderer::ShaderProgramSources CreateShaderPrograms(
        const std::vector<unsigned char>& rippleVertex,
        const std::vector<unsigned char>& rippleFragment) {
        return {
            {"button-wave", {
                {
                    reinterpret_cast<const char*>(rippleVertex.data()),
                    rippleVertex.size(),
                },
                {
                    reinterpret_cast<const char*>(rippleFragment.data()),
                    rippleFragment.size(),
                },
            }},
        };
    }
}