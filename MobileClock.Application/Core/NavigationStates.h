#pragma once
#include "../Model/AlarmRepository.h"
#include "Navigation.h"

#include <string>

namespace mobileclock::application::core {
    class AlarmEditNavigationState final : public base::NavigationStateBase {
    public:
        AlarmEditNavigationState(std::string alarmId, model::Alarm settings);

        const std::string& AlarmId() const;
        const model::Alarm& Settings() const;

    private:
        std::string alarmId;
        model::Alarm settings;
    };

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    class AlarmMelodyNavigationState final : public base::NavigationStateBase {
    public:
        explicit AlarmMelodyNavigationState(model::AlarmMelody alarmMelody);
        const model::AlarmMelody& Melody() const;
    private:
        model::AlarmMelody alarmMelody;
    };
#endif
}