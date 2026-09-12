#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include "UI/NavigationStates.h"

#include <utility>

namespace mobileclock::ui {
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
}
#endif