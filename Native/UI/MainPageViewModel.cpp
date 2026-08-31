#include "UI/MainPageViewModel.h"

#include "!Generated/Xaml/MainPage.xaml.h"
#include "Controls/ToggleSwitchControl.h"
#include "Controls/StackPanelControl.h"
#include "Controls/TextBlockControl.h"
#include "Controls/ControlChrome.h"
#include "Controls/BorderControl.h"
#include "Controls/ButtonControl.h"
#include "Controls/PageControl.h"
#include "Renderer/ControlRenderer.h"

#include <Helpers/platform/Android/Logging.h>

#include <algorithm>
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
    bool MainPageViewModel::IsAlarmEnabled() const {
        return this->isAlarmEnabled;
    }

    void MainPageViewModel::SetIsAlarmEnabled(bool value) {
        if (this->isAlarmEnabled == value) {
            return;
        }
        this->isAlarmEnabled = value;
        this->NotifyPropertyChanged(Property::isAlarmEnabled);
    }

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

    const std::vector<MainPageViewModel::Alarm>& MainPageViewModel::OtherAlarms() const {
        return this->otherAlarms;
    }

    void MainPageViewModel::Initialize(xaml::Size availableSize) {
        LOG_FUNCTION_SCOPE("MainPageViewModel::Initialize: {}x{}", availableSize.width, availableSize.height);
        this->controls.clear();
        this->capturedControl = nullptr;
        this->bindings.Clear();
        this->page = xaml::generated::MainPage::Create(*this, this->bindings);
        xaml::layout(*this->page, availableSize);
        const auto addControls = [this](xaml::Element& element, const auto& visit) -> void {
            if (element.Type() == xaml::ElementType::page) {
                this->controls.push_back(std::make_unique<PageControl>(element));
            } else if (element.Type() == xaml::ElementType::stackPanel
                || element.Type() == xaml::ElementType::grid
                || element.Type() == xaml::ElementType::listView) {
                this->controls.push_back(std::make_unique<StackPanelControl>(element));
            } else if (element.Type() == xaml::ElementType::button) {
                this->controls.push_back(std::make_unique<ButtonControl>(element));
            } else if (element.Type() == xaml::ElementType::border) {
                this->controls.push_back(std::make_unique<BorderControl>(element));
            } else if (element.Type() == xaml::ElementType::toggleSwitch) {
                this->controls.push_back(std::make_unique<ToggleSwitchControl>(element));
            } else if (element.Type() == xaml::ElementType::textBlock) {
                this->controls.push_back(std::make_unique<TextBlockControl>(element));
            }
            for (const auto& child : element.Children()) {
                visit(*child, visit);
            }
        };
        addControls(*this->page, addControls);
    }

    void MainPageViewModel::HandleTouchDown(float x, float y) {
        this->capturedControl = nullptr;
        for (auto control = this->controls.rbegin(); control != this->controls.rend(); ++control) {
            const xaml::Element& element = (*control)->ElementModel();
            if (element.VisibilityValue() != xaml::attr::Visibility::visible || !element.IsEnabled()) {
                continue;
            }
            if ((*control)->HitTest(x, y)) {
                this->capturedControl = control->get();
                return;
            }
        }
    }

    bool MainPageViewModel::HandleTouchUp(float x, float y) {
        IControl* const control = this->capturedControl;
        this->capturedControl = nullptr;
        if (control == nullptr || !control->HandleTap(x, y)) {
            return false;
        }
        this->bindings.UpdateSource(control->ElementModel());
        return true;
    }

    void MainPageViewModel::CancelTouch() {
        this->capturedControl = nullptr;
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
        const char clockText[9]{
            digit(localTime.tm_hour / 10),
            digit(localTime.tm_hour % 10),
            ':',
            digit(localTime.tm_min / 10),
            digit(localTime.tm_min % 10),
            ':',
            digit(localTime.tm_sec / 10),
            digit(localTime.tm_sec % 10),
            '\0',
        };
        this->SetClockText(clockText);
    }

    void MainPageViewModel::Render(mobileclock::renderer::ControlRenderer& renderer) const {
        for (const auto& control : this->controls) {
            if (control->ElementModel().VisibilityValue() != xaml::attr::Visibility::visible) {
                continue;
            }
            RenderControlChrome(control->ElementModel(), renderer);
            control->Render(renderer);
        }
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