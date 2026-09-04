#include <Helpers.Logging/Logging.h>
#include <XamlRuntime/RenderEngine.h>
#include <XamlRuntime/Animation.h>
#include <XamlRuntime/Input.h>

#include "../!Generated/Build/BuildVersion.h"
#include "../!Generated/Xaml/MainPage.xaml.h"
#include "MainPageViewModel.h"

#include <stdexcept>
#include <algorithm>
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

    MainPageViewModel::Alarm::Alarm(std::string time, std::string repeat, bool isEnabled)
        : time(std::move(time))
        , repeat(std::move(repeat))
        , isEnabled(isEnabled) {
    }

    const std::string& MainPageViewModel::Alarm::Time() const {
        return this->time;
    }

    const std::string& MainPageViewModel::Alarm::Repeat() const {
        return this->repeat;
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

    const std::string& MainPageViewModel::Status() const {
        return this->status;
    }

    const std::vector<MainPageViewModel::Alarm>& MainPageViewModel::Alarms() const {
        return this->alarms;
    }

    void MainPageViewModel::BindCommand(std::string name, CommandBindings::Handler handler) {
        this->commands.Bind(std::move(name), std::move(handler));
    }

    void MainPageViewModel::SetStatus(std::string value) {
        if (this->status == value) {
            return;
        }
        this->status = std::move(value);
        this->NotifyPropertyChanged(Property::status);
    }

    void MainPageViewModel::Initialize(xaml::Size availableSize) {
        LOG_FUNCTION_SCOPE("MobileClock", "MainPageViewModel::Initialize: {}x{}", availableSize.width, availableSize.height);
        this->capturedElement = nullptr;
        this->bindings.Clear();
        this->page = xaml::generated::MainPage::Create(*this, this->bindings);
        xaml::layout(*this->page, availableSize);
    }

    void MainPageViewModel::HandleTouchDown(
        float x,
        float y,
        xaml::AnimationController& animations) {
        this->capturedElement = xaml::HitTest(*this->page, x, y);
        if (this->capturedElement != nullptr) {
            animations.Start(*this->capturedElement, xaml::AnimationTrigger::pointerDown);
        }
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
        if (!xaml::HandleTap(*element)) {
            return TouchAction::none;
        }
        animations.Start(*element, xaml::AnimationTrigger::pointerUp);
        if (element->Type() == xaml::ElementType::toggleSwitch) {
            animations.Start(*element, xaml::AnimationTrigger::toggled);
        }
        this->bindings.UpdateSource(*element);
        if (element->Command() == "navigateToSettings") {
            return TouchAction::navigateToSettings;
        }
        this->commands.Execute(element->Command());
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