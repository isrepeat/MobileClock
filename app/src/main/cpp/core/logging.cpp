#include "core/logging.h"

#include <platform/Android/AndroidLogging.h>

namespace mobileclock::core {
void initializeLogging() {
    utility_helpers::android::initializeLogging("MobileClock");
}

spdlog::logger& log() {
    return utility_helpers::android::log();
}

} // namespace mobileclock::core
