#include "ApplicationSession.h"

#include "MobileClock.Presentation/Core/Registrations.h"

#include <utility>

namespace mobileclock::application::core {
    ApplicationSession::ApplicationSession(AppSessionController& appSessionController, model::AlarmRepository& alarmRepository, model::AlarmMelodyRepository& alarmMelodyRepository)
        : pageManager(appSessionController, alarmRepository, alarmMelodyRepository)
        , rendererRegistry() {
        mobileclock::presentation::core::RegisterRenderers(this->rendererRegistry);
    }

    //
    // API
    //
    void ApplicationSession::Initialize(xaml::Size availableSize) {
        this->pageManager.Initialize(availableSize);
    }

    void ApplicationSession::Resize(xaml::Size availableSize) {
        this->pageManager.Resize(availableSize);
    }

    void ApplicationSession::SetAnimationPlaybackRate(float value) {
        this->pageManager.SetAnimationPlaybackRate(value);
    }

    bool ApplicationSession::LoadPage(std::string_view name) {
        return this->pageManager.Navigate(name);
    }

    std::string_view ApplicationSession::CurrentPageName() const {
        return this->pageManager.CurrentPageName();
    }

    bool ApplicationSession::IsTransitioning() const {
        return this->pageManager.IsTransitioning();
    }

    void ApplicationSession::SetStatus(std::string value) {
        this->pageManager.SetStatus(std::move(value));
    }

    void ApplicationSession::AddAlarmMelody(model::AlarmMelody alarmMelody) {
        this->pageManager.AddAlarmMelody(std::move(alarmMelody));
    }

    void ApplicationSession::SetAlarmMelody(model::AlarmMelody alarmMelody) {
        this->pageManager.SetAlarmMelody(std::move(alarmMelody));
    }

#if defined(ANDROID_APP_PREVIEWER)
    bool ApplicationSession::preview_NavigateRoute(std::string_view target, std::string& error) {
        return this->pageManager.preview_NavigateRoute(target, error);
    }

    bool ApplicationSession::preview_NavigateTransitions(std::span<const std::string_view> transitionIds, std::string& error) {
        return this->pageManager.preview_NavigateTransitions(transitionIds, error);
    }

    bool ApplicationSession::preview_NavigateRoute(std::span<const std::string_view> path, std::string& error) {
        return this->pageManager.preview_NavigateRoute(path, error);
    }

    std::string ApplicationSession::preview_RouteGraph() const {
        return this->pageManager.preview_RouteGraph();
    }

    std::vector<PageManager::preview_Route> ApplicationSession::preview_Routes() const {
        return this->pageManager.preview_Routes();
    }

    std::string_view ApplicationSession::preview_PageTitle(std::string_view pageName) const {
        return this->pageManager.preview_PageTitle(pageName);
    }

    bool ApplicationSession::preview_ApplyScenario(std::string_view page, std::string_view json, std::string& error) {
        return this->pageManager.preview_ApplyScenario(page, json, error);
    }

    bool ApplicationSession::preview_ReloadMarkup(std::string_view page, std::string_view markup,
        std::string_view sourcePath, std::string& diagnostics) {
        return this->pageManager.preview_ReloadMarkup(page, markup, sourcePath, diagnostics);
    }
#endif

    void ApplicationSession::PointerDown(float x, float y) {
        this->pageManager.HandleTouchDown(x, y);
    }

    void ApplicationSession::PointerMove(float x, float y) {
        this->pageManager.HandleTouchMove(x, y);
    }

    void ApplicationSession::PointerUp(float x, float y) {
        this->pageManager.HandleTouchUp(x, y);
    }

    void ApplicationSession::CancelPointer() {
        this->pageManager.CancelTouch();
    }

    xaml::Element& ApplicationSession::Root() {
        return this->pageManager.Root();
    }

    void ApplicationSession::Update() {
        this->pageManager.UpdateClock();
    }

    void ApplicationSession::Render(xaml::IRenderBackend& renderer) const {
        this->pageManager.Render(renderer, this->rendererRegistry);
    }
}