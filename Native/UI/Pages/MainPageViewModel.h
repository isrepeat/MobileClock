#pragma once

#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Binding.h>

#include "UI/ApplicationActions.h"
#include "UI/Navigation.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace xaml {
    class IRenderBackend;
    class RendererRegistry;
}

namespace mobileclock::ui {
    class MainPageViewModel final {
    public:
        class Alarm final {
        public:
            Alarm(std::string time, std::string repeat, bool isEnabled);

            const std::string& Time() const;
            const std::string& Repeat() const;
            bool IsEnabled() const;
            xaml::Element::Command AlarmBlockCommand() const;
            xaml::Element::Command ToggleAlarmCommand() const;
            void SetToggleAlarmCommand(xaml::Element::Command value);

        private:
            std::string time;
            std::string repeat;
            bool isEnabled = false;
            xaml::Element::Command alarmBlockCommand = []() {};
            xaml::Element::Command toggleAlarmCommand;
        };

        enum class Property {
            clockText,
            packageVersion,
            status,
            isAlarmActionsMenuVisible,
        };

        using PropertyChangedHandler = std::function<void(Property)>;
        using Unsubscribe = std::function<void()>;

        MainPageViewModel(
            IPageNavigator& navigator,
            IApplicationActions& actions,
            std::function<void()> refreshPage);
        ~MainPageViewModel() = default;

        MainPageViewModel(const MainPageViewModel&) = delete;
        MainPageViewModel& operator=(const MainPageViewModel&) = delete;

        const std::string& ClockText() const;
        void SetClockText(std::string value);
        const std::string& PackageVersion() const;
        const std::string& Status() const;
        const std::vector<Alarm>& Alarms() const;
        bool IsAlarmActionsMenuVisible() const;
        void SetIsAlarmActionsMenuVisible(bool value);

        xaml::Element::Command CreateAlarmCommand() const;
        xaml::Element::Command NavigateToSettingsCommand() const;
        xaml::Element::Command ToggleAlarmCommand() const;
        xaml::Element::Command UpdateApplicationCommand() const;
        xaml::Element::Command UploadScreenshotCommand() const;
        xaml::Element::Command ToggleAlarmActionsMenuCommand() const;
        void SetStatus(std::string value);
        void Initialize(xaml::Size availableSize);
        void HandleTap(xaml::Element& element);
        bool HandleSwipe(const void* dataContext);
        void UpdateClock();
        void Render(xaml::IRenderBackend& renderer, const xaml::RendererRegistry& renderers) const;
        xaml::Element& Root();
        Unsubscribe Subscribe(PropertyChangedHandler handler);

    private:
        void ApplyAlarmActionsPanelState(bool useTransitions);
        void NotifyPropertyChanged(Property property);

    private:
        std::string clockText;
        std::string packageVersion;
        std::string status = "Готово к проверке обновлений";
        bool isAlarmActionsMenuVisible = false;
        std::vector<Alarm> alarms{
            {"05:55", "Пн, Вт, Ср, Чт, Пт", true},
            {"06:18", "Сб, Вс", false},
            {"06:30", "Ежедневно", true},
        };
        std::vector<PropertyChangedHandler> propertyChangedHandlers;
        std::unique_ptr<xaml::Element> page;
        xaml::BindingScope bindings;
        xaml::Element::Command createAlarmCommand;
        xaml::Element::Command navigateToSettingsCommand;
        xaml::Element::Command toggleAlarmCommand;
        xaml::Element::Command updateApplicationCommand;
        xaml::Element::Command uploadScreenshotCommand;
        xaml::Element::Command toggleAlarmActionsMenuCommand;
        std::function<void()> refreshPage;
    };
}