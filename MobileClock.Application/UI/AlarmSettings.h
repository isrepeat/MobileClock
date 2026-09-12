#pragma once
#include <string>
#include <array>

namespace mobileclock::ui {
    struct AlarmSettings final {
        int hour = 7;
        int minute = 30;
        std::array<bool, 7> days{true, true, true, true, true, false, false};
        std::string melody = "Рассвет";
        std::string melodyUri;
        bool vibration = true;
    };
}