#include "UI/PreviewScenario.h"

#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include "UI/ISerializable.h"

#include <string>

namespace mobileclock::ui {
    bool ApplyPreviewScenario(ISerializable& target, std::string_view json, std::string& error) {
        return target.Deserialize(json, error);
    }
}
#endif