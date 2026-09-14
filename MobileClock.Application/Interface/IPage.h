#pragma once
#include <XamlRuntime/XamlLayout.h>

#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeTreeBuilder.h>
#endif
#include "../Base/NavigationStateBase.h"

#include <string_view>
#include <string>
#include <memory>

namespace xaml {
    class IRenderBackend;
    class RendererRegistry;
}

namespace mobileclock::application::core {
    struct NavigationRequest;
}

namespace mobileclock::application::interface {
    class IPage {
    public:
        virtual ~IPage() = default;

        virtual std::string_view Name() const = 0;
        virtual std::unique_ptr<base::NavigationStateBase> OnNavigatingFrom(const core::NavigationRequest& request) = 0;
        virtual bool OnNavigatingTo(const core::NavigationRequest& request, std::unique_ptr<base::NavigationStateBase> state) = 0;
        virtual void Initialize(xaml::Size availableSize) = 0;
        virtual void HandleTap(xaml::Element& element) = 0;
        virtual void Update() = 0;
        virtual xaml::Element& Root() = 0;
        virtual void Render(xaml::IRenderBackend& renderer, const xaml::RendererRegistry& renderers) const = 0;
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        virtual std::string_view PreviewGraphTitle() const = 0;
        virtual bool ApplyScenario(std::string_view json, std::string& error) = 0;
        virtual xaml::runtime::RuntimeBindingContext RuntimeContext() = 0;
        virtual void ReplaceRuntimeTree(xaml::runtime::RuntimeBuildResult result) = 0;
#endif
    };
}