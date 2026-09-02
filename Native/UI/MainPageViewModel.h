#pragma once

#include "XamlRuntime/Binding.h"
#include "XamlRuntime/XamlLayout.h"

#include <functional>
#include <memory>
#include <vector>

namespace xaml {
    class IRenderBackend;
}

namespace mobileclock::ui {
    class MainPageViewModel final {
    public:
        class Alarm final {
        public:
            Alarm(std::string time, bool isEnabled);

            const std::string& Time() const;
            bool IsEnabled() const;

        private:
            std::string time;
            bool isEnabled = false;
        };

        enum class Property {
            clockText,
            packageVersion,
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
        void HandleTouchDown(float x, float y);
        bool HandleTouchUp(float x, float y);
        void CancelTouch();
        void UpdateClock();
        void Render(xaml::IRenderBackend& renderer) const;
        Unsubscribe Subscribe(PropertyChangedHandler handler);

    private:
        void NotifyPropertyChanged(Property property);

    private:
        std::string clockText;
        std::string packageVersion;
        const std::vector<Alarm> alarms{
            {"05:55", true},
            {"06:18", false},
            {"06:30", true},
            {"06:36", false},
        };
        std::vector<PropertyChangedHandler> propertyChangedHandlers;
        std::unique_ptr<xaml::Element> page;
        xaml::BindingScope bindings;
        xaml::Element* capturedElement = nullptr;
    };
}