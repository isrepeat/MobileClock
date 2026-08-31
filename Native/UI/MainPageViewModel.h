#pragma once

#include "Interfaces/IControl.h"
#include "UI/Binding.h"
#include "UI/IViewModel.h"
#include "XamlRuntime/XamlLayout.h"

#include <memory>
#include <vector>

namespace mobileclock::renderer {
    class ControlRenderer;
}

namespace mobileclock::ui {
    class MainPageViewModel final : public IViewModel {
    public:
        MainPageViewModel() = default;
        ~MainPageViewModel() = default;

        MainPageViewModel(const MainPageViewModel&) = delete;
        MainPageViewModel& operator=(const MainPageViewModel&) = delete;

    private:
        //
        // IViewModel
        //
        IViewModelValue GetValue(std::string_view path) const override;
        void SetValue(std::string_view path, IViewModelValue value) override;
        Unsubscribe Subscribe(PropertyChangedHandler handler) override;

    public:
        bool IsAlarmEnabled() const;
        void SetIsAlarmEnabled(bool value);
        const std::string& ClockText() const;
        void SetClockText(std::string value);

        void Initialize(Size availableSize);
        void HandleTouchDown(float x, float y);
        bool HandleTouchUp(float x, float y);
        void CancelTouch();
        void UpdateClock();
        void Render(mobileclock::renderer::ControlRenderer& renderer) const;

    private:
        void NotifyPropertyChanged(std::string_view path);

    private:
        bool isAlarmEnabled = true;
        std::string clockText;
        std::vector<PropertyChangedHandler> propertyChangedHandlers;
        std::unique_ptr<Element> page;
        std::vector<std::unique_ptr<IControl>> controls;
        BindingCollection bindings;
        IControl* capturedControl = nullptr;
    };
}