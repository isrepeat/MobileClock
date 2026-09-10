#pragma once
#include <XamlRuntime/RenderEngine.h>
#include <XamlRuntime/XamlLayout.h>

#include "UI/PageManager.h"

#include <string_view>
#include <string>

namespace mobileclock::ui {
    // Platform-neutral владелец UI-состояния одного запуска приложения.
    // Android и desktop previewer передают ему ввод и поверхность рендера,
    // но не создают страницы, bindings или animation registry самостоятельно.
    class ApplicationSession final {
    public:
        explicit ApplicationSession(IApplicationActions& actions);
        ~ApplicationSession() = default;

        ApplicationSession(const ApplicationSession&) = delete;
        ApplicationSession& operator=(const ApplicationSession&) = delete;

        void Initialize(xaml::Size availableSize);
        bool LoadPage(std::string_view name);
        void SetStatus(std::string value);
        void PointerDown(float x, float y);
        void PointerMove(float x, float y);
        void PointerUp(float x, float y);
        void CancelPointer();
        xaml::Element& Root();
        void Update();
        void Render(xaml::IRenderBackend& renderer) const;

    private:
        PageManager pageManager;
        xaml::RendererRegistry renderers;
    };
}