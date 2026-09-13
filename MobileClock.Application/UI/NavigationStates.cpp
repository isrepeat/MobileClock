#include "UI/NavigationStates.h"

#include <utility>

namespace mobileclock::ui {
    AlarmEditNavigationState::AlarmEditNavigationState(const void* alarm, AlarmSettings settings)
        : alarm(alarm)
        , settings(std::move(settings)) {
    }

    //
    // API
    //
    const void* AlarmEditNavigationState::Alarm() const {
        return this->alarm;
    }

    const AlarmSettings& AlarmEditNavigationState::Settings() const {
        return this->settings;
    }

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    AlarmMelodyNavigationState::AlarmMelodyNavigationState(std::string name, std::string uri)
        : name(std::move(name))
        , uri(std::move(uri)) {
    }

    //
    // API
    //
    const std::string& AlarmMelodyNavigationState::Name() const {
        return this->name;
    }

    const std::string& AlarmMelodyNavigationState::Uri() const {
        return this->uri;
    }
#endif
}