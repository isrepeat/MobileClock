#pragma once
#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include "UI/Navigation.h"

#include <string>

namespace mobileclock::ui {
    class AlarmMelodyNavigationState final : public NavigationState {
    public:
        AlarmMelodyNavigationState(std::string name, std::string uri);
        const std::string& Name() const;
        const std::string& Uri() const;
    private:
        std::string name;
        std::string uri;
    };
}
#endif