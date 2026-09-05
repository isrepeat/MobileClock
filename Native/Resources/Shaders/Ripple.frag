#version 300 es

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