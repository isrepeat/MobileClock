#include "UI/Pages/MainPageViewModel.h"

#include <Helpers.Logging/Logging.h>
#include <XamlRuntime/RenderEngine.h>
#include <XamlRuntime/Animation.h>

#include "!Generated/Xaml/Pages/MainPage.xaml.h"
#include "MobileClock.UI/Controls/AlarmList.h"
#include "!Generated/Build/BuildVersion.h"

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

    xaml::Element* FindElement(xaml::Element& element, std::string_view id) {
        if (element.Id() == id) {
            return &element;
        }
        for (const auto& child : element.Children()) {
            if (xaml::Element* const found = FindElement(*child, id)) {
                return found;
            }
        }
        return nullptr;
    }

    controls::AlarmList* FindAlarmList(xaml::Element& element) {
        if (auto* const alarmList = dynamic_cast<controls::AlarmList*>(&element)) {
            return alarmList;
        }
        for (const std::unique_ptr<xaml::Element>& child : element.Children()) {
            if (controls::AlarmList* const alarmList = FindAlarmList(*child)) {
                return alarmList;
            }
        }
        return nullptr;
    }
}

namespace mobileclock::ui {
    MainPageViewModel::MainPageViewModel(
        IPageNavigator& navigator,
        IApplicationActions& actions,
        std::function<void()> refreshPage)
        : packageVersion("v" MOBILECLOCK_PACKAGE_VERSION)
        , createAlarmCommand([this]() {
            this->alarms.emplace_back("", "", false);
            this->refreshPage();
        })
        , navigateToSettingsCommand([&navigator]() {
            navigator.Navigate(Page::settings);
        })
        , toggleAlarmCommand([&actions]() {
            actions.ToggleAlarm();
        })
        , updateApplicationCommand([&actions]() {
            actions.UpdateApplication();
        })
        , uploadScreenshotCommand([&actions]() {
            actions.UploadScreenshot();
        })
        , toggleAlarmActionsMenuCommand([this]() {
            this->SetIsAlarmActionsMenuVisible(!this->IsAlarmActionsMenuVisible());
        })
        , refreshPage(std::move(refreshPage)) {
        for (Alarm& alarm : this->alarms) {
            alarm.SetToggleAlarmCommand(this->toggleAlarmCommand);
        }
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

    xaml::Element::Command MainPageViewModel::Alarm::AlarmBlockCommand() const {
        return this->alarmBlockCommand;
    }

    xaml::Element::Command MainPageViewModel::Alarm::ToggleAlarmCommand() const {
        return this->toggleAlarmCommand;
    }

    void MainPageViewModel::Alarm::SetToggleAlarmCommand(xaml::Element::Command value) {
        this->toggleAlarmCommand = std::move(value);
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

    bool MainPageViewModel::IsAlarmActionsMenuVisible() const {
        return this->isAlarmActionsMenuVisible;
    }

    void MainPageViewModel::SetIsAlarmActionsMenuVisible(bool value) {
        if (this->isAlarmActionsMenuVisible == value) {
            return;
        }
        this->isAlarmActionsMenuVisible = value;
        this->NotifyPropertyChanged(Property::isAlarmActionsMenuVisible);
        if (this->page) {
            this->ApplyAlarmActionsPanelState(true);
        }
    }

    xaml::Element::Command MainPageViewModel::CreateAlarmCommand() const {
        return this->createAlarmCommand;
    }

    xaml::Element::Command MainPageViewModel::NavigateToSettingsCommand() const {
        return this->navigateToSettingsCommand;
    }

    xaml::Element::Command MainPageViewModel::ToggleAlarmCommand() const {
        return this->toggleAlarmCommand;
    }

    xaml::Element::Command MainPageViewModel::UpdateApplicationCommand() const {
        return this->updateApplicationCommand;
    }

    xaml::Element::Command MainPageViewModel::UploadScreenshotCommand() const {
        return this->uploadScreenshotCommand;
    }

    xaml::Element::Command MainPageViewModel::ToggleAlarmActionsMenuCommand() const {
        return this->toggleAlarmActionsMenuCommand;
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
        this->bindings.Clear();
        this->page = xaml::generated::MainPage::Create(*this, this->bindings);
        this->ApplyAlarmActionsPanelState(false);
        xaml::layout(*this->page, availableSize);
    }

    void MainPageViewModel::HandleTap(xaml::Element& element) {
        this->bindings.UpdateSource(element);
        element.ExecuteCommand();
    }

    bool MainPageViewModel::HandleSwipe(const void* dataContext) {
        const auto iterator = std::find_if(
            this->alarms.begin(),
            this->alarms.end(),
            [dataContext](const Alarm& alarm) {
                return &alarm == dataContext;
            });
        if (iterator == this->alarms.end()) {
            return false;
        }
        this->alarms.erase(iterator);
        this->refreshPage();
        return true;
    }

    controls::AlarmList& MainPageViewModel::AlarmList() {
        controls::AlarmList* const alarmList = _details::FindAlarmList(*this->page);
        if (alarmList == nullptr) {
            throw std::runtime_error("Alarm list control was not found");
        }
        return *alarmList;
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

    void MainPageViewModel::Render(
        xaml::IRenderBackend& renderer,
        const xaml::RendererRegistry& renderers) const {
        xaml::Render(*this->page, renderer, renderers);
    }

    xaml::Element& MainPageViewModel::Root() {
        return *this->page;
    }

    //
    // Internal
    //
    void MainPageViewModel::ApplyAlarmActionsPanelState(bool useTransitions) {
        xaml::Element* const host = _details::FindElement(*this->page, "alarmActionsHost");
        if (host == nullptr) {
            throw std::runtime_error("Alarm actions visual-state host was not found");
        }
        const char* const stateName = this->isAlarmActionsMenuVisible ? "Expanded" : "Collapsed";
        if (!xaml::VisualStateManager::GoToState(*host, "AlarmActionsPanelStates", stateName, useTransitions)) {
            throw std::runtime_error("Alarm actions visual state was not found");
        }
    }

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