#pragma once

#include "XamlRuntime/XamlLayout.h"
#include "XamlRuntime/Binding.h"
#include "Interfaces/IControl.h"

#include <functional>
#include <memory>
#include <vector>

namespace mobileclock::renderer {
    class ControlRenderer;
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
            isAlarmEnabled,
            clockText,
        };

        using PropertyChangedHandler = std::function<void(Property)>;
        using Unsubscribe = std::function<void()>;

        MainPageViewModel() = default;
        ~MainPageViewModel() = default;

        MainPageViewModel(const MainPageViewModel&) = delete;
        MainPageViewModel& operator=(const MainPageViewModel&) = delete;

        bool IsAlarmEnabled() const;
        void SetIsAlarmEnabled(bool value);
        const std::string& ClockText() const;
        void SetClockText(std::string value);
        const std::vector<Alarm>& OtherAlarms() const;

        void Initialize(xaml::Size availableSize);
        void HandleTouchDown(float x, float y);
        bool HandleTouchUp(float x, float y);
        void CancelTouch();
        void UpdateClock();
        void Render(mobileclock::renderer::ControlRenderer& renderer) const;
        Unsubscribe Subscribe(PropertyChangedHandler handler);

    private:
        void NotifyPropertyChanged(Property property);

    private:
        bool isAlarmEnabled = true;
        std::string clockText;
        const std::vector<Alarm> otherAlarms{
            {"06:18", true},
            {"06:30", true},
            {"06:36", true},
        };
        std::vector<PropertyChangedHandler> propertyChangedHandlers;
        std::unique_ptr<xaml::Element> page;
        std::vector<std::unique_ptr<IControl>> controls;
        xaml::BindingScope bindings;
        IControl* capturedControl = nullptr;
    };
}