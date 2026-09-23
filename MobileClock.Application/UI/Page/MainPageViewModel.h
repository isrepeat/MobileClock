#pragma once
#if defined(ANDROID_APP_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeTreeBuilder.h>
#endif
#include <XamlRuntime/ObservableCollection.h>
#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Binding.h>

#include "../../Interface/INavigationPage.h"
#include "../../Interface/ISerializable.h"
#include "../../Core/PageRegistry.h"
#include "../../Core/Navigation.h"
#include "../ViewModel/AlarmViewModel.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace xaml {
    class IRenderBackend;
    class RendererRegistry;
}

namespace mobileclock::application::ui::page {
    class MainPageViewModel final : public interface::ISerializable, public interface::INavigationPage {
    public:
        inline static constexpr std::string_view PageName = "MainPage";
        inline static constexpr std::string_view preview_GraphTitle = "⌂  Главная";

        enum class Property {
            clockText,
            packageVersion,
            status,
        };

        using PropertyChangedHandler = std::function<void(Property)>;
        using Unsubscribe = std::function<void()>;

        explicit MainPageViewModel(core::PageContext& context);
        ~MainPageViewModel() = default;

        MainPageViewModel(const MainPageViewModel&) = delete;
        MainPageViewModel& operator=(const MainPageViewModel&) = delete;

        //
        // ISerializable
        //
        std::string Serialize() const override;
        bool Deserialize(std::string_view json, std::string& error) override;
        //
        // INavigationPage
        //
        std::unique_ptr<base::NavigationStateBase> OnNavigatingFrom(const core::NavigationRequest& request) override;
        bool OnNavigatingTo(const core::NavigationRequest& request, std::unique_ptr<base::NavigationStateBase> state) override;

        const std::string& ClockText() const;
        void SetClockText(std::string value);
        const std::string& PackageVersion() const;
        const std::string& Status() const;
        const xaml::ObservableCollection<view_model::AlarmViewModel>& Alarms() const;

        void AddAlarm(const model::Alarm& alarmSettings);
        bool UpdateAlarm(const void* dataContext, const model::Alarm& alarmSettings);
        void CreateAlarm();
        void EditAlarm(const void* dataContext);
        void NavigateToSettings();
        xaml::Element::Command CreateAlarmCommand() const;
        xaml::Element::Command NavigateToSettingsCommand() const;
        xaml::Element::Command ToggleAlarmCommand() const;
        xaml::Element::Command UpdateApplicationCommand() const;
        xaml::Element::Command UploadScreenshotCommand() const;
        void SetStatus(std::string value);
        void Initialize(xaml::Size availableSize);
        void HandleTap(xaml::Element& element);
        bool RemoveItem(const void* dataContext);
        void Update();
        void Render(xaml::IRenderBackend& renderer, const xaml::RendererRegistry& renderers) const;
        xaml::Element& Root();
        Unsubscribe Subscribe(PropertyChangedHandler handler);

#if defined(ANDROID_APP_PREVIEWER)
        xaml::runtime::RuntimeBindingContext preview_RuntimeContext();
        void preview_ReplaceRuntimeTree(xaml::runtime::RuntimeBuildResult result);
#endif

    private:
        void NotifyPropertyChanged(Property property);
        void ConfigureAlarm(view_model::AlarmViewModel& alarm);
        void SetAlarmEnabled(view_model::AlarmViewModel& alarm, bool value);
        bool PersistAlarms();

    private:
        core::PageContext& context;
        std::string clockText;
        std::string packageVersion;
        std::string status = "Готово к проверке обновлений";
        xaml::ObservableCollection<view_model::AlarmViewModel> alarms{
            {"05:55", "Пн, Вт, Ср, Чт, Пт", true},
            {"06:18", "Сб, Вс", false},
            {"06:30", "Ежедневно", true},
        };
        std::vector<PropertyChangedHandler> propertyChangedHandlers;
        std::unique_ptr<xaml::Element> page;
        xaml::BindingScope bindings;
#if defined(ANDROID_APP_PREVIEWER)
        std::unique_ptr<xaml::BindingScope> runtimeBindings;
#endif
        xaml::Element::Command createAlarmCommand;
        xaml::Element::Command navigateToSettingsCommand;
        xaml::Element::Command toggleAlarmCommand;
        xaml::Element::Command updateApplicationCommand;
        xaml::Element::Command uploadScreenshotCommand;
    };
}