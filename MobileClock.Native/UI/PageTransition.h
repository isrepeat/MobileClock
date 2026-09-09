#pragma once
#include <string>

namespace mobileclock::ui {
    enum class NavigationDirection { forward, backward };

    struct PageTransitionData {
        std::string from;
        std::string to;
        NavigationDirection direction = NavigationDirection::forward;
    };
}