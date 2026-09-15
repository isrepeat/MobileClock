#pragma once
#include "../Base/NavigationStateBase.h"
#include "../Interface/INavigationPage.h"
#include "../Interface/IPageNavigator.h"

#include <string_view>
#include <memory>

namespace mobileclock::application::core {
    enum class NavigationTrigger {
        createAlarm,
        editAlarm,
        navigateToSettings,
        navigateToMain,
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        chooseAlarmMelody,
        applySelectedMelody,
        cancelMelodySelection,
#endif
    };

    struct NavigationRequest final {
        std::string_view source;
        std::string_view target;
        NavigationTrigger trigger;
    };

}