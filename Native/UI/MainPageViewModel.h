#pragma once

#include "XamlRuntime/Binding.h"
#include "XamlRuntime/XamlLayout.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace xaml {
    class AnimationController;
    class IRenderBackend;
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

        private:
            std::string time;
            std::string repeat;
            bool isEnabled = false;
        };

        enum class Property {
            clockText,
            packageVersion,
        };

        enum class TouchAction {
            none,
            contentChanged,
            navigateToSettings,
        };

        using PropertyChangedHandler = std::function<void(Property)>;
        using Unsubscribe = std::function<void()>;

        MainPageViewModel();
        ~MainPageViewModel() = default;

        MainPageViewModel(const MainPageViewModel&) = delete;
        MainPageViewModel& operator=(const MainPageViewModel&) = delete;

        const std::string& ClockText() const;
        void SetClockText(std::string value);
        const std::string& PackageVersion() const;
        const std::vector<Alarm>& Alarms() const;

        void Initialize(xaml::Size availableSize);
        void HandleTouchDown(float x, float y, xaml::AnimationController& animations);
        TouchAction HandleTouchUp(float x, float y, xaml::AnimationController& animations);
        void CancelTouch();
        void UpdateClock();
        void Render(xaml::IRenderBackend& renderer) const;
        xaml::Element& Root();
        Unsubscribe Subscribe(PropertyChangedHandler handler);

    private:
        void NotifyPropertyChanged(Property property);

    private:
        std::string clockText;
        std::string packageVersion;
        const std::vector<Alarm> alarms{
            {"05:55", "Пн, Вт, Ср, Чт, Пт", true},
            {"06:18", "Сб, Вс", false},
            {"06:30", "Ежедневно", true},
        };
        std::vector<PropertyChangedHandler> propertyChangedHandlers;
        std::unique_ptr<xaml::Element> page;
        xaml::BindingScope bindings;
        xaml::Element* capturedElement = nullptr;
    };
}