#include "PreviewSession.h"

#include "Effects/Effects.h"
#include "Effects/Shaders.h"
#include "AnimationRenderers.h"
#include "PageTransition.h"

#include <string>

namespace mobileclock::presentation {
    //
    // API
    //
    void PreviewSession::Attach(xaml::Element& root) {
        xaml::AnimationRegistry registry = mobileclock::resources::effects::CreateAnimations();
        mobileclock::renderer::RegisterAnimations(registry);
        this->animations.Attach(root, registry, false);
    }

    void PreviewSession::SetPageTransition(
        xaml::Element& root,
        std::string_view from,
        std::string_view to,
        bool backward,
        bool visible) {
        const PageTransitionData data{
            std::string(from),
            std::string(to),
            backward ? NavigationDirection::backward
                     : NavigationDirection::forward,
        };
        root.SetAnimationParametersProvider([data]() {
            return xaml::AnimationParameters::Create(data);
        });
        root.SetVisibility(visible ? xaml::attr::Visibility::visible : xaml::attr::Visibility::collapsed);
    }

    void PreviewSession::SetPlaybackRate(float value) {
        this->animations.SetPlaybackRate(value);
    }

    bool PreviewSession::Update() {
        const bool wasAnimating = this->animations.IsAnimating();
        this->animations.Update();
        return wasAnimating || this->animations.IsAnimating();
    }

    xaml::AnimationController& PreviewSession::Animations() {
        return this->animations;
    }

    xaml::RendererRegistry PreviewSession::CreateRenderers() {
        xaml::RendererRegistry renderers = mobileclock::resources::effects::CreateRenderers();
        mobileclock::renderer::RegisterAnimationRenderers(renderers);
        return renderers;
    }

    es_renderer::OpenGlRenderer::ShaderProgramSources PreviewSession::CreateShaderPrograms() {
        return mobileclock::resources::effects::CreateShaderPrograms();
    }
}