#include "UI/NavigationStates.h"

#include <utility>

namespace mobileclock::ui {
    AlarmEditNavigationState::AlarmEditNavigationState(std::string alarmId, Alarm settings)
        : alarmId(std::move(alarmId))
        , settings(std::move(settings)) {
    }

    //
    // API
    //
    const std::string& AlarmEditNavigationState::AlarmId() const {
        return this->alarmId;
    }

    const Alarm& AlarmEditNavigationState::Settings() const {
        return this->settings;
    }

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    AlarmMelodyNavigationState::AlarmMelodyNavigationState(AlarmMelody alarmMelody)
        : alarmMelody(std::move(alarmMelody)) {
    }

    //
    // API
    //
    const AlarmMelody& AlarmMelodyNavigationState::Melody() const {
        return this->alarmMelody;
    }
#endif
}