#pragma once
#include <XamlRuntime/XamlLayout.h>

#include "../../Model/AlarmRepository.h"

#include <string>

namespace mobileclock::application::ui::view_model {
    class AlarmViewModel final {
    public:
        AlarmViewModel(
            std::string time,
            std::string repeat,
            bool isEnabled,
            model::AlarmMelody melody = {});
        AlarmViewModel(std::string id, model::Alarm alarm, bool isEnabled);
        explicit AlarmViewModel(const model::Alarm& alarm);
        ~AlarmViewModel() = default;

        const std::string& Id() const;
        const model::Alarm& Settings() const;
        const std::string& Time() const;
        const std::string& Repeat() const;
        bool IsEnabled() const;
        xaml::Element::Command AlarmBlockCommand() const;
        xaml::Element::Command ToggleAlarmCommand() const;

        void SetIsEnabled(bool value);
        void SetAlarmBlockCommand(xaml::Element::Command value);
        void SetToggleAlarmCommand(xaml::Element::Command value);

    private:
        std::string id;
        model::Alarm alarm;
        std::string time;
        std::string repeat;
        bool isEnabled = false;
        xaml::Element::Command alarmBlockCommand = []() {};
        xaml::Element::Command toggleAlarmCommand;
    };
}