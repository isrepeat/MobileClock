#pragma once
#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeTreeBuilder.h>
#endif
#include <XamlRuntime/ObservableCollection.h>
#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Binding.h>

#include "UI/ISerializable.h"
#include "UI/PageRegistry.h"
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
    class MainPageViewModel final : public ISerializable, public INavigationPage {
    public:
        inline static constexpr std::string_view PageName = "MainPage";
        inline static constexpr std::string_view PreviewGraphTitle = "⌂  Главная";

        class Alarm final {
        public:
            Alarm(std::string time, std::string repeat, bool isEnabled);
            explicit Alarm(const AlarmSettings& settings);

            const AlarmSettings& Settings() const;

            const std::string& Time() const;
            const std::string& Repeat() const;
            bool IsEnabled() const;
            void SetIsEnabled(bool value);
            xaml::Element::Command AlarmBlockCommand() const;
            xaml::Element::Command ToggleAlarmCommand() const;
            void SetToggleAlarmCommand(xaml::Element::Command value);

        private:
            AlarmSettings settings;
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
        };

        using PropertyChangedHandler = std::function<void(Property)>;
        using Unsubscribe = std::function<void()>;

        explicit MainPageViewModel(PageContext& context);
        ~MainPageViewModel() = default;

        MainPageViewModel(const MainPageViewModel&) = delete;
        MainPageViewModel& operator=(const MainPageViewModel&) = delete;

#if defined(MOBILECLOCK_XAML_PREVIEWER)
        //
        // ISerializable
        //
        bool Deserialize(std::string_view json, std::string& error) override;
#endif
        //
        // INavigationPage
        //
        std::unique_ptr<NavigationState> OnNavigatingFrom(const NavigationRequest& request) override;
        bool OnNavigatingTo(const NavigationRequest& request, std::unique_ptr<NavigationState> state) override;

        const std::string& ClockText() const;
        void SetClockText(std::string value);
        const std::string& PackageVersion() const;
        const std::string& Status() const;
        const xaml::ObservableCollection<Alarm>& Alarms() const;

        void AddAlarm(const AlarmSettings& settings);
        void CreateAlarm();
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

#if defined(MOBILECLOCK_XAML_PREVIEWER)
        xaml::runtime::RuntimeBindingContext RuntimeContext();
        void ReplaceRuntimeTree(xaml::runtime::RuntimeBuildResult result);
#endif

    private:
        void NotifyPropertyChanged(Property property);

    private:
        PageContext& context;
        std::string clockText;
        std::string packageVersion;
        std::string status = "Готово к проверке обновлений";
        xaml::ObservableCollection<Alarm> alarms{
            {"05:55", "Пн, Вт, Ср, Чт, Пт", true},
            {"06:18", "Сб, Вс", false},
            {"06:30", "Ежедневно", true},
        };
        std::vector<PropertyChangedHandler> propertyChangedHandlers;
        std::unique_ptr<xaml::Element> page;
        xaml::BindingScope bindings;
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        std::unique_ptr<xaml::BindingScope> runtimeBindings;
#endif
        xaml::Element::Command createAlarmCommand;
        xaml::Element::Command navigateToSettingsCommand;
        xaml::Element::Command toggleAlarmCommand;
        xaml::Element::Command updateApplicationCommand;
        xaml::Element::Command uploadScreenshotCommand;
    };
}