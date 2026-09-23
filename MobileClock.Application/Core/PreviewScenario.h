#pragma once
#if defined(ANDROID_APP_PREVIEWER)
#include "../Interface/ISerializable.h"

#include <string_view>
#include <string>

namespace mobileclock::application::core {
    bool preview_ApplyScenario(interface::ISerializable& target, std::string_view json, std::string& error);
}
#endif