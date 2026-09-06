#include "Shaders.h"

namespace mobileclock::resources::effects::_details {
    constexpr char ButtonWaveVertexShader[] = R"(#version 300 es

        layout (location = 0) in vec2 position;
        layout (location = 1) in vec2 localPosition;

        out vec2 local;

        void main() {
            local = localPosition;
            gl_Position = vec4(position, 0.0, 1.0);
        }
    )";

    constexpr char ButtonWaveFragmentShader[] = R"(#version 300 es

        precision mediump float;

        in vec2 local;

        uniform vec2 size;
        uniform float cornerRadius;
        uniform float progress;
        uniform float spread;
        uniform vec4 rippleColor;

        out vec4 color;

        void main() {
            vec2 halfSize = size * 0.5;
            float radius = min(cornerRadius, min(halfSize.x, halfSize.y));
            vec2 cornerDistance = abs(local * size - halfSize) - (halfSize - radius);
            if (length(max(cornerDistance, 0.0)) - radius > 0.0) {
                discard;
            }

            float distanceFromCenter = length((local - vec2(0.5)) * size);
            float pulseRadius = 8.0 + progress * length(size) * spread;
            float normalizedDistance = distanceFromCenter / pulseRadius;
            float glow = exp(-normalizedDistance * normalizedDistance * 3.5);
            color = vec4(rippleColor.rgb, rippleColor.a * glow);
        }
    )";

}

namespace mobileclock::resources::effects {
    es_renderer::OpenGlRenderer::ShaderProgramSources CreateShaderPrograms() {
        return {
            {"button-wave", {_details::ButtonWaveVertexShader, _details::ButtonWaveFragmentShader}},
        };
    }
}