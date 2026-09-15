#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include "PreviewScenario.h"

#include "../Interface/ISerializable.h"

#include <string>

namespace mobileclock::application::core {
    bool ApplyPreviewScenario(interface::ISerializable& target, std::string_view json, std::string& error) {
        return target.Deserialize(json, error);
    }
}
#endif