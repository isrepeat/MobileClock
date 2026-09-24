#if defined(ANDROID_APP_PREVIEWER)
#include "PreviewScenario.h"

#include "../Interface/ISerializable.h"

#include <string>

namespace mobileclock::application::core {
    bool preview_ApplyScenario(interface::ISerializable& target, std::string_view json, std::string& error) {
        return target.Deserialize(json, error);
    }
}
#endif