#pragma once
#include <XamlRuntime/RenderEngine.h>
#include <XamlRuntime/XamlLayout.h>

#include "PageManager.h"

#include <string_view>
#include <string>

namespace mobileclock::application::core {
    class AppSessionController;
    // Platform-neutral владелец UI-состояния одного запуска приложения.
    // Android и desktop previewer передают ему ввод и поверхность рендера,
    // но не создают страницы, bindings или animation registry самостоятельно.
    class ApplicationSession final {
    public:
        ApplicationSession(AppSessionController& appSessionController, model::AlarmRepository& alarmRepository, model::AlarmMelodyRepository& alarmMelodyRepository);
        ~ApplicationSession() = default;

        ApplicationSession(const ApplicationSession&) = delete;
        ApplicationSession& operator=(const ApplicationSession&) = delete;

        void Initialize(xaml::Size availableSize);
        void Resize(xaml::Size availableSize);
        void SetAnimationPlaybackRate(float value);
        bool LoadPage(std::string_view name);
        std::string_view CurrentPageName() const;
        bool IsTransitioning() const;
        void SetStatus(std::string value);
        void AddAlarmMelody(model::AlarmMelody alarmMelody);
        void SetAlarmMelody(model::AlarmMelody alarmMelody);
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        bool NavigatePreviewRoute(std::string_view target, std::string& error);
        bool NavigatePreviewRoute(std::span<const std::string_view> path, std::string& error);
        std::string PreviewRouteGraph() const;
        std::string_view PreviewPageTitle(std::string_view pageName) const;
        bool ApplyPreviewScenario(std::string_view page, std::string_view json, std::string& error);
        bool ReloadMarkup(std::string_view page, std::string_view markup, std::string_view sourcePath, std::string& diagnostics);
#endif
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