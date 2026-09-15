#pragma once
#include <XamlRuntime/RenderEngine.h>
#include <XamlRuntime/Animation.h>
#include <ESRenderer/OpenGlRenderer.h>

#include <string_view>

namespace xaml {
    class Element;
}

namespace mobileclock::presentation::core {
    class PreviewSession final {
    public:
        PreviewSession() = default;
        ~PreviewSession() = default;

        PreviewSession(const PreviewSession&) = delete;
        PreviewSession& operator=(const PreviewSession&) = delete;

        void Attach(xaml::Element& root);
        void SetPageTransition(
            xaml::Element& root,
            std::string_view from,
            std::string_view to,
            bool backward,
            bool visible);
        void SetPlaybackRate(float value);
        bool Update();

        xaml::AnimationController& Animations();

        static xaml::RendererRegistry CreateRenderers();
        static es_renderer::OpenGlRenderer::ShaderProgramSources CreateShaderPrograms();

    private:
        xaml::AnimationController animations;
    };
}