#include "ApplicationSession.h"

#include "MobileClock.Presentation/Core/Registrations.h"

#include <utility>

namespace mobileclock::application::core {
    ApplicationSession::ApplicationSession(AppSessionController& appSessionController, model::AlarmRepository& alarmRepository, model::AlarmMelodyRepository& alarmMelodyRepository)
        : pageManager(appSessionController, alarmRepository, alarmMelodyRepository)
        , renderers() {
        mobileclock::presentation::core::RegisterRenderers(this->renderers);
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

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    bool ApplicationSession::NavigatePreviewRoute(std::string_view target, std::string& error) {
        return this->pageManager.NavigatePreviewRoute(target, error);
    }

    bool ApplicationSession::NavigatePreviewTransitions(std::span<const std::string_view> transitionIds, std::string& error) {
        return this->pageManager.NavigatePreviewTransitions(transitionIds, error);
    }

    bool ApplicationSession::NavigatePreviewRoute(std::span<const std::string_view> path, std::string& error) {
        return this->pageManager.NavigatePreviewRoute(path, error);
    }

    std::string ApplicationSession::PreviewRouteGraph() const {
        return this->pageManager.PreviewRouteGraph();
    }

    std::vector<PageManager::PreviewRoute> ApplicationSession::PreviewRoutes() const {
        return this->pageManager.PreviewRoutes();
    }

    std::string_view ApplicationSession::PreviewPageTitle(std::string_view pageName) const {
        return this->pageManager.PreviewPageTitle(pageName);
    }

    bool ApplicationSession::ApplyPreviewScenario(std::string_view page, std::string_view json, std::string& error) {
        return this->pageManager.ApplyPreviewScenario(page, json, error);
    }

    bool ApplicationSession::ReloadMarkup(std::string_view page, std::string_view markup,
        std::string_view sourcePath, std::string& diagnostics) {
        return this->pageManager.ReloadMarkup(page, markup, sourcePath, diagnostics);
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
        this->pageManager.Render(renderer, this->renderers);
    }
}