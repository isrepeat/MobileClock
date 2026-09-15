#pragma once
#include <XamlRuntime/RenderEngine.h>
#include <XamlRuntime/Animation.h>
#include <ESRenderer/OpenGlRenderer.h>

namespace mobileclock::presentation::core {
    // Registers visual capabilities shared by every MobileClock host.
    // Session state deliberately does not live in this library.
    void RegisterAnimations(xaml::AnimationRegistry& registry);
    void RegisterRenderers(xaml::RendererRegistry& registry);
    es_renderer::OpenGlRenderer::ShaderProgramSources CreateShaderPrograms();
}