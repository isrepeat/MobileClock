#include "MainPageViewModel.h"

#include <Helpers.Logging/Logging.h>
#include <XamlRuntime/RenderEngine.h>

#if defined(ANDROID_APP_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeBindingPublisher.h>
#endif
#include <JsonParser/json_struct/json_struct.h>

#if defined(ANDROID_APP_PREVIEWER)
#include "MobileClock.UI/Control/AlarmActionsMenu.h"
#include "MobileClock.UI/Control/TimelineTabs.h"
#include "MobileClock.UI/Control/AlarmList.h"
#endif
#include "!Generated/MobileClock.Application/Xaml/Page/MainPage.xaml.h"
#include "!Generated/Build/BuildVersion.h"
#include "../../Core/AppSessionController.h"
#include "../../Core/NavigationStates.h"
#include "AddAlarmPageViewModel.h"
#include "SettingsPageViewModel.h"

#include <algorithm>
#include <stdexcept>
#include <atomic>
#include <chrono>
#include <format>
#include <tuple>
#include <ctime>

namespace mobileclock::application::ui::page::_details {
    struct MainPageSerializationDocument final {
        std::optional<std::string> Status = "Готово к проверке обновлений";
        std::optional<std::vector<model::Alarm>> Alarms = std::vector<model::Alarm>{};
        std::optional<std::vector<model::AlarmMelody>> AlarmMelodies = std::vector<model::AlarmMelody>{};

        JS_OBJECT(
            JS_MEMBER(Status),
            JS_MEMBER(Alarms),
            JS_MEMBER(AlarmMelodies)
        );
    };


    bool TryGetLocalTime(std::time_t value, std::tm& result) {
#if defined(_WIN32)
        return localtime_s(&result, &value) == 0;
#else
        return localtime_r(&value, &result) != nullptr;
#endif
    }

} // namespace _details

namespace mobileclock::application::ui::page {
    MainPageViewModel::MainPageViewModel(core::PageContext& context)
        : context(context)
        , packageVersion("v" MOBILECLOCK_PACKAGE_VERSION)
        , createAlarmCommand([this]() {
            this->CreateAlarm();
        })
        , navigateToSettingsCommand([this]() {
            this->NavigateToSettings();
        })
        , toggleAlarmCommand([&context]() {
            context.appSessionController.Dispatch(core::AppSessionSignal::toggleAlarm, {});
        })
        , updateApplicationCommand([&context]() {
            context.appSessionController.Dispatch(core::AppSessionSignal::updateApplication, {});
        })
        , uploadScreenshotCommand([&context]() {
            context.appSessionController.Dispatch(core::AppSessionSignal::uploadScreenshot, {});
        }) {
        if (!this->context.alarmRepository.Alarms().empty()) {
            this->alarms.Clear();
            for (const model::Alarm& alarm : this->context.alarmRepository.Alarms()) {
                this->alarms.EmplaceBack(alarm.id, alarm, alarm.isEnabled);
            }
        }
        for (view_model::AlarmViewModel& alarm : this->alarms) {
            this->ConfigureAlarm(alarm);
        }
    }

    //
    // ISerializable
    //
    std::string MainPageViewModel::Serialize() const {
        _details::MainPageSerializationDocument scenario;
        scenario.Status = this->status;
        scenario.Alarms = this->context.alarmRepository.Alarms();
        scenario.AlarmMelodies = this->context.alarmMelodyRepository.Melodies();
        return JS::serializeStruct(scenario);
    }

    bool MainPageViewModel::Deserialize(std::string_view json, std::string& error) {
        _details::MainPageSerializationDocument scenario;
        JS::ParseContext context(json.data(), json.size());
        if (context.parseTo(scenario) != JS::Error::NoError) {
            error = std::format("Invalid serialized JSON: {}", context.makeErrorString());
            return false;
        }
        if (scenario.Alarms) {
            // Сценарий сначала превращается в полный документ хранилища, чтобы UI
            // работал с тем же состоянием и id, что и обычное приложение.
            model::ApplicationStateDocument document;
            document.alarmMelodies = std::move(*scenario.AlarmMelodies);
            for (size_t index = 0; index < scenario.Alarms->size(); ++index) {
                model::Alarm alarm = (*scenario.Alarms)[index];
                if (alarm.hour < 0 || alarm.hour > 23 || alarm.minute < 0 || alarm.minute > 59) {
                    error = "Preview scenario contains an invalid alarm time";
                    return false;
                }
                // Id сценария намеренно не используется: повторное применение
                // одного сценария всегда создаёт одинаковые preview-alarm-N.
                alarm.id = std::format("preview-alarm-{}", index + 1);
                document.alarms.push_back(std::move(alarm));
            }
            // Документ остаётся в памяти preview-сеанса и не перезаписывает
            // постоянный storage, пока его явно не экспортируют.
            this->context.alarmRepository.preview_LoadScenarioState(std::move(document));
            // Репозитории кэшируют свои коллекции, поэтому после замены полного
            // документа оба кэша синхронизируются, а MainPage пересобирает UI.
            this->context.alarmRepository.preview_ReloadFromStateStore();
            this->context.alarmMelodyRepository.preview_ReloadFromStateStore();
            this->OnNavigatingTo({}, {});
        }
        if (scenario.Status) {
            this->SetStatus(std::move(*scenario.Status));
        }
        return true;
    }


    //
    // INavigationPage
    //
    std::unique_ptr<base::NavigationStateBase> MainPageViewModel::OnNavigatingFrom(const core::NavigationRequest&) {
        return {};
    }

    bool MainPageViewModel::OnNavigatingTo(const core::NavigationRequest&, std::unique_ptr<base::NavigationStateBase>) {
        if (!this->context.alarmRepository.Alarms().empty()) {
            this->alarms.Clear();
            for (const model::Alarm& alarm : this->context.alarmRepository.Alarms()) {
                view_model::AlarmViewModel& value = this->alarms.EmplaceBack(alarm.id, alarm, alarm.isEnabled);
                this->ConfigureAlarm(value);
            }
        }
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

    const xaml::ObservableCollection<view_model::AlarmViewModel>& MainPageViewModel::Alarms() const {
        return this->alarms;
    }

    void MainPageViewModel::AddAlarm(const model::Alarm& alarmSettings) {
        if (this->context.alarmRepository.CreateAlarm(alarmSettings)) {
            this->OnNavigatingTo({}, {});
        }
    }

    bool MainPageViewModel::UpdateAlarm(const void* dataContext, const model::Alarm& alarmSettings) {
        const auto alarm = std::find_if(this->alarms.begin(), this->alarms.end(), [dataContext](const view_model::AlarmViewModel& value) { return &value == dataContext; });
        if (alarm == this->alarms.end() || !this->context.alarmRepository.UpdateAlarm(alarm->Id(), alarmSettings)) {
            return false;
        }
        this->OnNavigatingTo({}, {}); return true;
    }

    void MainPageViewModel::CreateAlarm() {
        this->context.navigator.Trigger(core::NavigationTrigger::createAlarm);
    }

    void MainPageViewModel::EditAlarm(const void* dataContext) {
        const auto alarm = std::find_if(this->alarms.begin(), this->alarms.end(), [dataContext](const view_model::AlarmViewModel& value) {
            return &value == dataContext;
        });
        if (alarm == this->alarms.end()) {
            return;
        }
        // Данные принадлежат конкретному действию, а не временному полю страницы.
        // Тот же контракт используется previewer-ом для построения валидного перехода.
        this->context.navigator.Trigger(
            core::NavigationTrigger::editAlarm,
            std::make_unique<core::AlarmEditNavigationState>(alarm->Id(), alarm->Settings()));
    }

    void MainPageViewModel::NavigateToSettings() {
        this->context.navigator.Trigger(core::NavigationTrigger::navigateToSettings);
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
#if defined(ANDROID_APP_PREVIEWER)
        this->runtimeBindings.reset();
#endif
        this->page = xaml::generated::MainPage::Create(*this, this->bindings);
        xaml::layout(*this->page, availableSize);
    }

    void MainPageViewModel::HandleTap(xaml::Element& element) {
        this->bindings.UpdateSource(element);
#if defined(ANDROID_APP_PREVIEWER)
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
            [dataContext](const view_model::AlarmViewModel& alarm) {
                return &alarm == dataContext;
            });
        if (iterator == this->alarms.end()) {
            return false;
        }
        if (!this->context.alarmRepository.RemoveAlarm(iterator->Id())) { return false; }
        this->alarms.Erase(iterator); return true;
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

#if defined(ANDROID_APP_PREVIEWER)
    //
    // API
    //
    xaml::runtime::RuntimeBindingContext MainPageViewModel::preview_RuntimeContext() {
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
        result.xamlNamespace = "urn:mobileclock:xaml";
        result.controlXmlNamespace = "using:mobileclock.ui.control";
        xaml::runtime::RuntimeCollectionDescriptor collection;
        // collection.count и collection.at пока не подключены RuntimeTreeBuilder.
        // Текущий путь через collection.bind вызывает SetItemsSource, который сам
        // получает размер коллекции и элементы из this->alarms.
        collection.itemBindings = [this](const void* value) {
            const auto* alarm = static_cast<const view_model::AlarmViewModel*>(value);
            auto item = std::make_shared<xaml::runtime::RuntimeBindingRegistry>();
            item->AddText("Time", [alarm]() { return alarm == nullptr ? "" : alarm->Time(); });
            item->AddText("Repeat", [alarm]() { return alarm == nullptr ? "" : alarm->Repeat(); });
            item->AddBoolean("IsEnabled", [alarm]() { return alarm != nullptr && alarm->IsEnabled(); }, {},
                [this, alarm](bool value) {
                    if (alarm != nullptr) {
                        this->SetAlarmEnabled(*const_cast<view_model::AlarmViewModel*>(alarm), value);
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
        result.controls["AlarmList"] = [this](xaml::BindingScope& scope) {
            return mobileclock::ui::control::AlarmList::Create(*this, this->alarms, scope);
        };
        result.controls["TimelineTabs"] = [this](xaml::BindingScope& scope) {
            return mobileclock::ui::control::TimelineTabs::Create(*this, scope);
        };
        result.controls["AlarmActionsMenu"] = [this](xaml::BindingScope& scope) {
            return mobileclock::ui::control::AlarmActionsMenu::Create(*this, scope);
        };
        return result;
    }

    void MainPageViewModel::preview_ReplaceRuntimeTree(xaml::runtime::RuntimeBuildResult result) {
        mobileclock::ui::control::AlarmActionsMenu::preview_PreserveState(*this->page, *result.root);
        mobileclock::ui::control::AlarmList::preview_PreserveInstances(*this->page, *result.root, *result.bindings);
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

    void MainPageViewModel::ConfigureAlarm(view_model::AlarmViewModel& alarm) {
        alarm.SetAlarmBlockCommand([this, &alarm]() {
            this->EditAlarm(&alarm);
        });
        alarm.SetToggleAlarmCommand(this->toggleAlarmCommand);
    }

    void MainPageViewModel::SetAlarmEnabled(view_model::AlarmViewModel& alarm, bool value) {
        if (alarm.IsEnabled() == value) {
            return;
        }
        if (this->context.alarmRepository.SetAlarmEnabled(alarm.Id(), value)) { alarm.SetIsEnabled(value); }
    }

    bool MainPageViewModel::PersistAlarms() {
        return true;
    }
}