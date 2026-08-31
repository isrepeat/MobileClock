#include "UI/MainPageViewModel.h"

#include "!Generated/Xaml/MainPage.xaml.h"
#include "Controls/BorderControl.h"
#include "Controls/ButtonControl.h"
#include "Controls/PageControl.h"
#include "Controls/TextBlockControl.h"
#include "Controls/ToggleSwitchControl.h"
#include "Renderer/ControlRenderer.h"

#include <Helpers/platform/Android/Logging.h>

#include <algorithm>
#include <ctime>
#include <stdexcept>

namespace mobileclock::ui {
    //
    // IViewModel
    //
    IViewModelValue MainPageViewModel::GetValue(std::string_view path) const {
        if (path == "IsAlarmEnabled") {
            return this->IsAlarmEnabled();
        }
        if (path == "ClockText") {
            return this->ClockText();
        }
        throw std::invalid_argument("Unknown MainPageViewModel property: " + std::string(path));
    }

    void MainPageViewModel::SetValue(std::string_view path, IViewModelValue value) {
        if (path == "IsAlarmEnabled") {
            this->SetIsAlarmEnabled(std::get<bool>(value));
            return;
        }
        if (path == "ClockText") {
            this->SetClockText(std::get<std::string>(std::move(value)));
            return;
        }
        throw std::invalid_argument("Unknown MainPageViewModel property: " + std::string(path));
    }

    IViewModel::Unsubscribe MainPageViewModel::Subscribe(PropertyChangedHandler handler) {
        this->propertyChangedHandlers.push_back(std::move(handler));
        const size_t index = this->propertyChangedHandlers.size() - 1;
        return [this, index]() {
            this->propertyChangedHandlers[index] = nullptr;
        };
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
        this->NotifyPropertyChanged("IsAlarmEnabled");
    }

    const std::string& MainPageViewModel::ClockText() const {
        return this->clockText;
    }

    void MainPageViewModel::SetClockText(std::string value) {
        if (this->clockText == value) {
            return;
        }
        this->clockText = std::move(value);
        this->NotifyPropertyChanged("ClockText");
    }

    void MainPageViewModel::Initialize(Size availableSize) {
        LOG_FUNCTION_SCOPE("MainPageViewModel::Initialize: {}x{}", availableSize.width, availableSize.height);
        this->page = generated::MainPage::Create();
        this->controls.clear();
        this->capturedControl = nullptr;
        this->bindings.Connect(*this->page, *this);
        layout(*this->page, availableSize);
        const auto addControls = [this](Element& element, const auto& visit) -> void {
            if (element.Type() == ElementType::page) {
                this->controls.push_back(std::make_unique<PageControl>(element));
            } else if (element.Type() == ElementType::button) {
                this->controls.push_back(std::make_unique<ButtonControl>(element));
            } else if (element.Type() == ElementType::border) {
                this->controls.push_back(std::make_unique<BorderControl>(element));
            } else if (element.Type() == ElementType::toggleSwitch) {
                this->controls.push_back(std::make_unique<ToggleSwitchControl>(element));
            } else if (element.Type() == ElementType::textBlock) {
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
        this->bindings.UpdateSource(control->ElementModel(), *this);
        return true;
    }

    void MainPageViewModel::CancelTouch() {
        this->capturedControl = nullptr;
    }

    void MainPageViewModel::UpdateClock() {
        const std::time_t now = std::time(nullptr);
        std::tm localTime{};
        localtime_r(&now, &localTime);
        char clockText[9]{};
        std::strftime(clockText, sizeof(clockText), "%H:%M:%S", &localTime);
        this->SetClockText(clockText);
    }

    void MainPageViewModel::Render(mobileclock::renderer::ControlRenderer& renderer) const {
        for (const auto& control : this->controls) {
            control->Render(renderer);
        }
    }

    //
    // Internal
    //
    void MainPageViewModel::NotifyPropertyChanged(std::string_view path) {
        for (const PropertyChangedHandler& handler : this->propertyChangedHandlers) {
            if (handler) {
                handler(path);
            }
        }
    }
}