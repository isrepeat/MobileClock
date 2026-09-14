#pragma once
#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include "../Interface/ISerializable.h"

#include <string_view>
#include <string>

namespace mobileclock::application::core {
    bool ApplyPreviewScenario(interface::ISerializable& target, std::string_view json, std::string& error);
}
#endif