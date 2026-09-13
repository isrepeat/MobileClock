#pragma once
#include "UI/Navigation.h"
#include "UI/AlarmSettings.h"

#include <string>

namespace mobileclock::ui {
    class AlarmEditNavigationState final : public NavigationState {
    public:
        AlarmEditNavigationState(const void* alarm, AlarmSettings settings);

        const void* Alarm() const;
        const AlarmSettings& Settings() const;

    private:
        const void* alarm = nullptr;
        AlarmSettings settings;
    };

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    class AlarmMelodyNavigationState final : public NavigationState {
    public:
        AlarmMelodyNavigationState(std::string name, std::string uri);
        const std::string& Name() const;
        const std::string& Uri() const;
    private:
        std::string name;
        std::string uri;
    };
#endif
}