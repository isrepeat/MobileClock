#pragma once
#include <XamlRuntime/XamlLayout.h>

#include "../../Model/AlarmRepository.h"

#include <string>

namespace mobileclock::application::ui::view_model {
    class AlarmMelodyViewModel final {
    public:
        explicit AlarmMelodyViewModel(
            model::AlarmMelody alarmMelody,
            xaml::Element::Command selectCommand = {},
            xaml::Element::Command deleteCommand = {});
        ~AlarmMelodyViewModel() = default;

        const std::string& Id() const;
        const std::string& Name() const;
        const std::string& Uri() const;
        const model::AlarmMelody& Value() const;
        xaml::Element::Command SelectCommand() const;
        xaml::Element::Command DeleteCommand() const;

        void SetName(std::string value);

    private:
        model::AlarmMelody alarmMelody;
        xaml::Element::Command selectCommand;
        xaml::Element::Command deleteCommand;
    };
}