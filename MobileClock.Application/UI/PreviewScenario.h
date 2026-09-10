#pragma once

#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <string_view>
#include <string>

namespace mobileclock::ui {
    class ISerializable;

    bool ApplyPreviewScenario(ISerializable& target, std::string_view json, std::string& error);
}
#endif