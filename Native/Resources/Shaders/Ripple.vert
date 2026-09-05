#version 300 es

layout (location = 0) in vec2 position;
layout (location = 1) in vec2 localPosition;

out vec2 local;

void main() {
    local = localPosition;
    gl_Position = vec4(position, 0.0, 1.0);
}