#include "UI/MainPageViewModel.h"

#include <Helpers/platform/Android/Logging.h>
#include <XamlRuntime/Input.h>
#include <XamlRuntime/Animation.h>
#include <XamlRuntime/RenderEngine.h>

#include "!Generated/Build/BuildVersion.h"
#include "!Generated/Xaml/MainPage.xaml.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <ctime>

namespace mobileclock::ui::_details {
    bool TryGetLocalTime(std::time_t value, std::tm& result) {
#if defined(_WIN32)
        return localtime_s(&result, &value) == 0;
#else
        return localtime_r(&value, &result) != nullptr;
#endif
    }
}

namespace mobileclock::ui {
    MainPageViewModel::MainPageViewModel()
        : packageVersion("v" MOBILECLOCK_PACKAGE_VERSION) {
    }

    MainPageViewModel::Alarm::Alarm(std::string time, bool isEnabled)
        : time(std::move(time))
        , isEnabled(isEnabled) {
    }

    const std::string& MainPageViewModel::Alarm::Time() const {
        return this->time;
    }

    bool MainPageViewModel::Alarm::IsEnabled() const {
        return this->isEnabled;
    }

    //
    // API
    //
    const std::string& MainPageViewModel::ClockText() const {
        return this->clockText;
    }

    void MainPageViewModel::SetClockText(std::string value) {
        if (this->clockText == value) {
            return;
        }
        this->clockText = std::move(value);
        this->NotifyPropertyChanged(Property::clockText);
    }

    const std::string& MainPageViewModel::PackageVersion() const {
        return this->packageVersion;
    }

    const std::vector<MainPageViewModel::Alarm>& MainPageViewModel::Alarms() const {
        return this->alarms;
    }

    void MainPageViewModel::Initialize(xaml::Size availableSize) {
        LOG_FUNCTION_SCOPE("MainPageViewModel::Initialize: {}x{}", availableSize.width, availableSize.height);
        this->capturedElement = nullptr;
        this->bindings.Clear();
        this->page = xaml::generated::MainPage::Create(*this, this->bindings);
        xaml::layout(*this->page, availableSize);
    }

    void MainPageViewModel::HandleTouchDown(float x, float y) {
        this->capturedElement = xaml::HitTest(*this->page, x, y);
    }

    MainPageViewModel::TouchAction MainPageViewModel::HandleTouchUp(
        float x,
        float y,
        xaml::AnimationController& animations) {
        xaml::Element* const element = this->capturedElement;
        this->capturedElement = nullptr;
        if (element == nullptr) {
            return TouchAction::none;
        }
        const bool wasOn = element->IsOn();
        if (!xaml::HandleTap(*element)) {
            return TouchAction::none;
        }
        if (element->Type() == xaml::ElementType::toggleSwitch) {
            animations.Animate(
                *element,
                xaml::AnimatedProperty::toggleProgress,
                wasOn ? 1.0f : 0.0f,
                wasOn ? 0.0f : 1.0f,
                std::chrono::milliseconds(120));
        }
        if (element->Id() == "settingsButton") {
            return TouchAction::navigateToSettings;
        }
        this->bindings.UpdateSource(*element);
        return TouchAction::contentChanged;
    }

    void MainPageViewModel::CancelTouch() {
        this->capturedElement = nullptr;
    }

    void MainPageViewModel::UpdateClock() {
        const std::time_t now = std::time(nullptr);
        std::tm localTime{};
        if (!_details::TryGetLocalTime(now, localTime)) {
            return;
        }
        const auto digit = [](int value) {
            return static_cast<char>('0' + value);
        };
        const char clockText[6]{
            digit(localTime.tm_hour / 10),
            digit(localTime.tm_hour % 10),
            ':',
            digit(localTime.tm_min / 10),
            digit(localTime.tm_min % 10),
            '\0',
        };
        this->SetClockText(clockText);
    }

    void MainPageViewModel::Render(xaml::IRenderBackend& renderer) const {
        xaml::Render(*this->page, renderer);
    }

    xaml::Element& MainPageViewModel::Root() {
        return *this->page;
    }

    //
    // Internal
    //
    MainPageViewModel::Unsubscribe MainPageViewModel::Subscribe(PropertyChangedHandler handler) {
        this->propertyChangedHandlers.push_back(std::move(handler));
        const size_t index = this->propertyChangedHandlers.size() - 1;
        return [this, index]() {
            this->propertyChangedHandlers[index] = nullptr;
        };
    }

    void MainPageViewModel::NotifyPropertyChanged(Property property) {
        for (const PropertyChangedHandler& handler : this->propertyChangedHandlers) {
            if (handler) {
                handler(property);
            }
        }
    }
}