#include "UI/SettingsPageViewModel.h"

#include <XamlRuntime/Input.h>
#include <XamlRuntime/Animation.h>
#include <XamlRuntime/RenderEngine.h>

#include "!Generated/Xaml/SettingsPage.xaml.h"

#include <utility>

namespace mobileclock::ui {
    //
    // API
    //
    const std::string& SettingsPageViewModel::Theme() const {
        return this->theme;
    }

    const std::string& SettingsPageViewModel::Sound() const {
        return this->sound;
    }

    void SettingsPageViewModel::Initialize(xaml::Size availableSize) {
        this->capturedElement = nullptr;
        this->bindings.Clear();
        this->page = xaml::generated::SettingsPage::Create(*this, this->bindings);
        xaml::layout(*this->page, availableSize);
    }

    void SettingsPageViewModel::HandleTouchDown(float x, float y) {
        this->capturedElement = xaml::HitTest(*this->page, x, y);
    }

    SettingsPageViewModel::TouchAction SettingsPageViewModel::HandleTouchUp(
        float x,
        float y,
        xaml::AnimationController& animations) {
        xaml::Element* const element = this->capturedElement;
        this->capturedElement = nullptr;
        if (element == nullptr) {
            return TouchAction::none;
        }
        if (!xaml::HandleTap(*element)) {
            return TouchAction::none;
        }
        if (element->Type() == xaml::ElementType::toggleSwitch) {
            animations.Start(*element, xaml::AnimationTrigger::toggled);
        }
        animations.Start(*element, xaml::AnimationTrigger::pointerUp);
        if (element->Id() == "backNavigation") {
            return TouchAction::navigateToMain;
        }
        this->bindings.UpdateSource(*element);
        return TouchAction::none;
    }

    void SettingsPageViewModel::CancelTouch() {
        this->capturedElement = nullptr;
    }

    void SettingsPageViewModel::Render(xaml::IRenderBackend& renderer) const {
        xaml::Render(*this->page, renderer);
    }

    xaml::Element& SettingsPageViewModel::Root() {
        return *this->page;
    }

    SettingsPageViewModel::Unsubscribe SettingsPageViewModel::Subscribe(PropertyChangedHandler handler) {
        this->propertyChangedHandlers.push_back(std::move(handler));
        const size_t index = this->propertyChangedHandlers.size() - 1;
        return [this, index]() {
            this->propertyChangedHandlers[index] = nullptr;
        };
    }
}