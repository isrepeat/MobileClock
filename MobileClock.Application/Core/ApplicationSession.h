#pragma once
#include <XamlRuntime/RenderEngine.h>
#include <XamlRuntime/XamlLayout.h>

#include "PageManager.h"

#include <string_view>
#include <string>
#include <vector>

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
#if defined(ANDROID_APP_PREVIEWER)
        bool preview_NavigateRoute(std::string_view target, std::string& error);
        bool preview_NavigateTransitions(std::span<const std::string_view> transitionIds, std::string& error);
        bool preview_NavigateRoute(std::span<const std::string_view> path, std::string& error);
        std::string preview_RouteGraph() const;
        std::vector<PageManager::preview_Route> preview_Routes() const;
        std::string_view preview_PageTitle(std::string_view pageName) const;
        bool preview_ApplyScenario(std::string_view page, std::string_view json, std::string& error);
        bool preview_ReloadMarkup(std::string_view page, std::string_view markup, std::string_view sourcePath, std::string& diagnostics);
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
        xaml::RendererRegistry rendererRegistry;
    };
}