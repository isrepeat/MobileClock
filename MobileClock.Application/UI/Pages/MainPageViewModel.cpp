#include "UI/Pages/MainPageViewModel.h"

#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include "MobileClock.UI/Controls/AlarmActionsMenu.h"
#include "MobileClock.UI/Controls/InteractiveList.h"
#include "MobileClock.UI/Controls/TimelineTabs.h"
#endif

#include <Helpers.Logging/Logging.h>
#include <XamlRuntime/RenderEngine.h>

#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeBindingPublisher.h>
#include <JsonParser/JsonParser.h>
#endif

#include "!Generated/MobileClock.Application/Xaml/Pages/MainPage.xaml.h"
#include "!Generated/Build/BuildVersion.h"
#include "UI/Pages/SettingsPageViewModel.h"
#include "UI/Pages/AddAlarmPageViewModel.h"

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
        std::optional<std::vector<PreviewAlarm>> Alarms = std::vector<PreviewAlarm>{};

        JS_OBJECT(JS_MEMBER(Status), JS_MEMBER(Alarms));
    };
#endif

    bool TryGetLocalTime(std::time_t value, std::tm& result) {
#if defined(_WIN32)
        return localtime_s(&result, &value) == 0;
#else
        return localtime_r(&value, &result) != nullptr;
#endif
    }

}

namespace mobileclock::ui {
    MainPageViewModel::MainPageViewModel(PageContext& context)
        : context(context)
        , packageVersion("v" MOBILECLOCK_PACKAGE_VERSION)
        , createAlarmCommand([this]() {
            this->CreateAlarm();
        })
        , navigateToSettingsCommand([this]() {
            this->NavigateToSettings();
        })
        , toggleAlarmCommand([&context]() {
            context.actions.ToggleAlarm();
        })
        , updateApplicationCommand([&context]() {
            context.actions.UpdateApplication();
        })
        , uploadScreenshotCommand([&context]() {
            context.actions.UploadScreenshot();
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

    MainPageViewModel::Alarm::Alarm(const AlarmSettings& settings)
        : settings(settings)
        , time(std::format("{:02}:{:02}", settings.hour, settings.minute))
        , isEnabled(true) {
        constexpr std::array<std::string_view, 7> names{"Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс"};
        for (size_t index = 0; index < settings.days.size(); ++index) {
            if (settings.days[index]) {
                if (!this->repeat.empty()) {
                    this->repeat += ", ";
                }
                this->repeat += names[index];
            }
        }
        if (this->repeat.empty()) {
            this->repeat = "Однократно";
        }
        else if (std::all_of(settings.days.begin(), settings.days.end(), [](bool day) { return day; })) {
            this->repeat = "Ежедневно";
        }
    }

    //
    // API
    //
    const AlarmSettings& MainPageViewModel::Alarm::Settings() const {
        return this->settings;
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

    void MainPageViewModel::Alarm::SetIsEnabled(bool value) {
        this->isEnabled = value;
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

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    //
    // ISerializable
    //
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
        return true;
    }
#endif

    //
    // INavigationPage
    //
    std::unique_ptr<NavigationState> MainPageViewModel::OnNavigatingFrom(const NavigationRequest&) {
        return {};
    }

    bool MainPageViewModel::OnNavigatingTo(const NavigationRequest&, std::unique_ptr<NavigationState>) {
        return true;
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

    void MainPageViewModel::AddAlarm(const AlarmSettings& settings) {
        Alarm& alarm = this->alarms.EmplaceBack(settings);
        alarm.SetToggleAlarmCommand(this->toggleAlarmCommand);
    }

    void MainPageViewModel::CreateAlarm() {
        this->context.navigator.Trigger(NavigationTrigger::createAlarm);
    }

    void MainPageViewModel::NavigateToSettings() {
        this->context.navigator.Trigger(NavigationTrigger::navigateToSettings);
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
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        this->runtimeBindings.reset();
#endif
        this->page = xaml::generated::MainPage::Create(*this, this->bindings);
        xaml::layout(*this->page, availableSize);
    }

    void MainPageViewModel::HandleTap(xaml::Element& element) {
        this->bindings.UpdateSource(element);
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        if (this->runtimeBindings) {
            this->runtimeBindings->UpdateSource(element);
        }
#endif
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

    MainPageViewModel::Unsubscribe MainPageViewModel::Subscribe(PropertyChangedHandler handler) {
        this->propertyChangedHandlers.push_back(std::move(handler));
        const size_t index = this->propertyChangedHandlers.size() - 1;
        return [this, index]() {
            this->propertyChangedHandlers[index] = nullptr;
        };
    }

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    //
    // API
    //
    xaml::runtime::RuntimeBindingContext MainPageViewModel::RuntimeContext() {
        auto registry = std::make_shared<xaml::runtime::RuntimeBindingRegistry>();
        xaml::runtime::RuntimeBindingPublisher publisher{*registry, *this};
        publisher.Text("Status", Property::status, &MainPageViewModel::Status);
        publisher.Text("ClockText", Property::clockText, &MainPageViewModel::ClockText);
        publisher.Text("PackageVersion", Property::packageVersion, &MainPageViewModel::PackageVersion);
        publisher.Command("CreateAlarmCommand", &MainPageViewModel::CreateAlarmCommand);
        publisher.Command("NavigateToSettingsCommand", &MainPageViewModel::NavigateToSettingsCommand);
        publisher.Command("ToggleAlarmCommand", &MainPageViewModel::ToggleAlarmCommand);
        publisher.Command("UpdateApplicationCommand", &MainPageViewModel::UpdateApplicationCommand);
        publisher.Command("UploadScreenshotCommand", &MainPageViewModel::UploadScreenshotCommand);
        xaml::runtime::RuntimeBindingContext result{registry, "MainPageViewModel", {}};
        xaml::runtime::RuntimeCollectionDescriptor collection;
        // collection.count и collection.at пока не подключены RuntimeTreeBuilder.
        // Текущий путь через collection.bind вызывает SetItemsSource, который сам
        // получает размер коллекции и элементы из this->alarms.
        collection.itemBindings = [](const void* value) {
            const auto* alarm = static_cast<const Alarm*>(value);
            auto item = std::make_shared<xaml::runtime::RuntimeBindingRegistry>();
            item->AddText("Time", [alarm]() { return alarm == nullptr ? "" : alarm->Time(); });
            item->AddText("Repeat", [alarm]() { return alarm == nullptr ? "" : alarm->Repeat(); });
            item->AddBoolean("IsEnabled", [alarm]() { return alarm != nullptr && alarm->IsEnabled(); }, {},
                [alarm](bool value) {
                    if (alarm != nullptr) {
                        const_cast<Alarm*>(alarm)->SetIsEnabled(value);
                    }
                });
            item->AddCommand("AlarmBlockCommand", alarm == nullptr ? xaml::Element::Command{} : alarm->AlarmBlockCommand());
            item->AddCommand("ToggleAlarmCommand", alarm == nullptr ? xaml::Element::Command{} : alarm->ToggleAlarmCommand());
            return item;
        };
        collection.bind = [this](xaml::Element& element, xaml::Element::ItemTemplate itemTemplate) {
            element.SetItemsSource(this->alarms, std::move(itemTemplate));
        };
        // Если в runtime-XAML встретится {Binding Alarms} в itemsSource, используй этот RuntimeCollectionDescriptor.
        registry->AddCollection("Alarms", collection);
        registry->AddCollection("ItemsSource", collection);
        result.controls["InteractiveList"] = [this](xaml::BindingScope& scope) {
            return controls::InteractiveList::Create(*this, this->alarms, scope);
        };
        result.controls["TimelineTabs"] = [this](xaml::BindingScope& scope) {
            return controls::TimelineTabs::Create(*this, scope);
        };
        result.controls["AlarmActionsMenu"] = [this](xaml::BindingScope& scope) {
            return controls::AlarmActionsMenu::Create(*this, scope);
        };
        return result;
    }

    void MainPageViewModel::ReplaceRuntimeTree(xaml::runtime::RuntimeBuildResult result) {
        controls::AlarmActionsMenu::PreserveState(*this->page, *result.root);
        controls::InteractiveList::PreserveInstances(*this->page, *result.root, *result.bindings);
        this->bindings.Clear();
        this->runtimeBindings.reset();
        this->page = std::move(result.root);
        this->runtimeBindings = std::move(result.bindings);
    }
#endif

    //
    // Internal
    //
    void MainPageViewModel::NotifyPropertyChanged(Property property) {
        for (const PropertyChangedHandler& handler : this->propertyChangedHandlers) {
            if (handler) {
                handler(property);
            }
        }
    }
}