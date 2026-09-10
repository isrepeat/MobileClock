#include "MobileClock.Presentation/Registrations.h"

#include "UI/ApplicationSession.h"

#include <utility>

namespace mobileclock::ui {
    ApplicationSession::ApplicationSession(IApplicationActions& actions)
        : pageManager(actions)
        , renderers() {
        mobileclock::presentation::RegisterRenderers(this->renderers);
    }

    //
    // API
    //
    void ApplicationSession::Initialize(xaml::Size availableSize) {
        this->pageManager.Initialize(availableSize);
    }

    bool ApplicationSession::LoadPage(std::string_view name) {
        if (name == "MainPage" || name == "main") {
            this->pageManager.Navigate(Page::main);
            return true;
        }
        if (name == "SettingsPage" || name == "settings") {
            this->pageManager.Navigate(Page::settings);
            return true;
        }
        return false;
    }

    void ApplicationSession::SetStatus(std::string value) {
        this->pageManager.SetStatus(std::move(value));
    }

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