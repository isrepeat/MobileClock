#include "AlarmMelodyViewModel.h"

#include <utility>

namespace mobileclock::application::ui::view_model {
    AlarmMelodyViewModel::AlarmMelodyViewModel(
        model::AlarmMelody alarmMelody,
        xaml::Element::Command selectCommand,
        xaml::Element::Command deleteCommand)
        : alarmMelody(std::move(alarmMelody))
        , selectCommand(std::move(selectCommand))
        , deleteCommand(std::move(deleteCommand)) {
    }

    //
    // API
    //
    const std::string& AlarmMelodyViewModel::Id() const {
        return this->alarmMelody.id;
    }

    const std::string& AlarmMelodyViewModel::Name() const {
        return this->alarmMelody.name;
    }

    const std::string& AlarmMelodyViewModel::Uri() const {
        return this->alarmMelody.uri;
    }

    const model::AlarmMelody& AlarmMelodyViewModel::Value() const {
        return this->alarmMelody;
    }

    xaml::Element::Command AlarmMelodyViewModel::SelectCommand() const {
        return this->selectCommand;
    }

    xaml::Element::Command AlarmMelodyViewModel::DeleteCommand() const {
        return this->deleteCommand;
    }

    void AlarmMelodyViewModel::SetName(std::string value) {
        this->alarmMelody.name = std::move(value);
    }
}