#include "core/logging.h"

#include <platform/Android/AndroidLogging.h>

namespace mobileclock::core {
void configureLogFile(std::string_view path) {
    utility_helpers::android::configureLogFile(path);
}

void initializeLogging() {
    utility_helpers::android::initializeLogging("MobileClock");
}

spdlog::logger& log() {
    return utility_helpers::android::log();
}

void flushLogging() {
    utility_helpers::android::flushLogging();
}

} // namespace mobileclock::core