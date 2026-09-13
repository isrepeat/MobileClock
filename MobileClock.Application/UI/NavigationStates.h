#pragma once
#include "Storage/AlarmRepository.h"
#include "UI/Navigation.h"

#include <string>

namespace mobileclock::ui {
    class AlarmEditNavigationState final : public NavigationState {
    public:
        AlarmEditNavigationState(std::string alarmId, Alarm settings);

        const std::string& AlarmId() const;
        const Alarm& Settings() const;

    private:
        std::string alarmId;
        Alarm settings;
    };

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    class AlarmMelodyNavigationState final : public NavigationState {
    public:
        explicit AlarmMelodyNavigationState(AlarmMelody alarmMelody);
        const AlarmMelody& Melody() const;
    private:
        AlarmMelody alarmMelody;
    };
#endif
}