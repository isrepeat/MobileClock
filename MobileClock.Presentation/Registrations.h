#pragma once
#include <ESRenderer/OpenGlRenderer.h>
#include <XamlRuntime/Animation.h>
#include <XamlRuntime/RenderEngine.h>

namespace mobileclock::presentation {
    // Registers visual capabilities shared by every MobileClock host.
    // Session state deliberately does not live in this library.
    void RegisterAnimations(xaml::AnimationRegistry& registry);
    void RegisterRenderers(xaml::RendererRegistry& registry);
    es_renderer::OpenGlRenderer::ShaderProgramSources CreateShaderPrograms();
}