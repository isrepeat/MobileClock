#pragma once
#include <string>

namespace mobileclock::presentation::core {
    enum class NavigationDirection {
        forward,
        backward,
    };

    struct PageTransitionData {
        std::string from;
        std::string to;
        NavigationDirection direction = NavigationDirection::forward;
    };
}