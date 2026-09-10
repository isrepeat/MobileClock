#include "UI/Pages/MainPageViewModel.h"

#include <Helpers.Logging/Logging.h>
#include <XamlRuntime/RenderEngine.h>
#include <XamlRuntime/Animation.h>

#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <JsonParser/JsonParser.h>
#endif

#include "!Generated/MobileClock.Application/Xaml/Pages/MainPage.xaml.h"
#include "UI/Pages/SettingsPageViewModel.h"
#include "!Generated/Build/BuildVersion.h"

#include <stdexcept>
#include <algorithm>
#include <format>
#include <ctime>

namespace mobileclock::ui::_details {
#if defined(MOBILECLOCK_XAML_PREVIEWER)
    struct PreviewAlarm final {
        std::string Time;
        std::string Repeat;
        bool IsEnabled = false;

        JS_OBJECT(JS_MEMBER(Time), JS_MEMBER(Repeat), JS_MEMBER(IsEnabled));
    };

    struct MainPagePreviewScenario final {
        std::optional<std::string> Status = "Готово к проверке обновлений";
        std::optional<bool> IsAlarmActionsMenuVisible = false;
        std::optional<std::vector<PreviewAlarm>> Alarms = std::vector<PreviewAlarm>{};

        JS_OBJECT(JS_MEMBER(Status), JS_MEMBER(IsAlarmActionsMenuVisible), JS_MEMBER(Alarms));
    };
#endif

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

}

namespace mobileclock::ui {
    MainPageViewModel::MainPageViewModel(PageContext& context)
        : packageVersion("v" MOBILECLOCK_PACKAGE_VERSION)
        , createAlarmCommand([this]() {
            Alarm& alarm = this->alarms.EmplaceBack("", "", false);
            alarm.SetToggleAlarmCommand(this->toggleAlarmCommand);
        })
        , navigateToSettingsCommand([&context]() {
            context.navigator.Navigate<SettingsPageViewModel>();
        })
        , toggleAlarmCommand([&context]() {
            context.actions.ToggleAlarm();
        })
        , updateApplicationCommand([&context]() {
            context.actions.UpdateApplication();
        })
        , uploadScreenshotCommand([&context]() {
            context.actions.UploadScreenshot();
        })
        , toggleAlarmActionsMenuCommand([this]() {
            this->SetIsAlarmActionsMenuVisible(!this->IsAlarmActionsMenuVisible());
        }) {
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

    const xaml::ObservableCollection<MainPageViewModel::Alarm>& MainPageViewModel::Alarms() const {
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

    bool MainPageViewModel::RemoveItem(const void* dataContext) {
        const auto iterator = std::find_if(
            this->alarms.begin(),
            this->alarms.end(),
            [dataContext](const Alarm& alarm) {
                return &alarm == dataContext;
            });
        if (iterator == this->alarms.end()) {
            return false;
        }
        this->alarms.Erase(iterator);
        return true;
    }

    void MainPageViewModel::Update() {
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

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    bool MainPageViewModel::Deserialize(std::string_view json, std::string& error) {
        _details::MainPagePreviewScenario scenario;
        JS::ParseContext context(json.data(), json.size());
        if (context.parseTo(scenario) != JS::Error::NoError) {
            error = std::format("Invalid preview scenario JSON: {}", context.makeErrorString());
            return false;
        }
        if (scenario.Alarms) {
            this->alarms.Clear();
            for (const _details::PreviewAlarm& alarmValue : *scenario.Alarms) {
                Alarm& alarm = this->alarms.EmplaceBack(alarmValue.Time, alarmValue.Repeat, alarmValue.IsEnabled);
                alarm.SetToggleAlarmCommand(this->toggleAlarmCommand);
            }
        }
        if (scenario.Status) {
            this->SetStatus(std::move(*scenario.Status));
        }
        if (scenario.IsAlarmActionsMenuVisible) {
            this->SetIsAlarmActionsMenuVisible(*scenario.IsAlarmActionsMenuVisible);
        }
        return true;
    }
#endif

    void MainPageViewModel::NotifyPropertyChanged(Property property) {
        for (const PropertyChangedHandler& handler : this->propertyChangedHandlers) {
            if (handler) {
                handler(property);
            }
        }
    }
}