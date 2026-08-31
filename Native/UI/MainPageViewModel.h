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
        std::vector<PropertyChangedHandler> propertyChangedHandlers;
        std::unique_ptr<xaml::Element> page;
        std::vector<std::unique_ptr<IControl>> controls;
        xaml::BindingScope bindings;
        IControl* capturedControl = nullptr;
    };
}