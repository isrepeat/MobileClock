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
        navigateBack,
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        chooseAlarmMelody,
#endif
    };

    enum class NavigationTargetKind {
        page,
        // Цель определяется предпоследней записью фактической истории PageManager.
        previousPage,
    };

    struct NavigationRequest final {
        std::string_view source;
        std::string_view target;
        NavigationTrigger trigger;
    };

}