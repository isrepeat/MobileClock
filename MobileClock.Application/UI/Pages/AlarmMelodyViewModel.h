#pragma once
#include <XamlRuntime/XamlLayout.h>

#include "Storage/AlarmRepository.h"

#include <string>

namespace mobileclock::ui {
    class AlarmMelodyViewModel final {
    public:
        explicit AlarmMelodyViewModel(
            AlarmMelody alarmMelody,
            xaml::Element::Command selectCommand = {},
            xaml::Element::Command deleteCommand = {});
        ~AlarmMelodyViewModel() = default;

        const std::string& Id() const;
        const std::string& Name() const;
        const std::string& Uri() const;
        const AlarmMelody& Value() const;
        xaml::Element::Command SelectCommand() const;
        xaml::Element::Command DeleteCommand() const;

        void SetName(std::string value);

    private:
        AlarmMelody alarmMelody;
        xaml::Element::Command selectCommand;
        xaml::Element::Command deleteCommand;
    };
}