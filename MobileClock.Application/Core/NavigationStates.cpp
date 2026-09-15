#include "NavigationStates.h"

#include <utility>

namespace mobileclock::application::core {
    AlarmEditNavigationState::AlarmEditNavigationState(std::string alarmId, model::Alarm settings)
        : alarmId(std::move(alarmId))
        , settings(std::move(settings)) {
    }

    //
    // API
    //
    const std::string& AlarmEditNavigationState::AlarmId() const {
        return this->alarmId;
    }

    const model::Alarm& AlarmEditNavigationState::Settings() const {
        return this->settings;
    }

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    AlarmMelodyNavigationState::AlarmMelodyNavigationState(model::AlarmMelody alarmMelody)
        : alarmMelody(std::move(alarmMelody)) {
    }

    //
    // API
    //
    const model::AlarmMelody& AlarmMelodyNavigationState::Melody() const {
        return this->alarmMelody;
    }
#endif
}