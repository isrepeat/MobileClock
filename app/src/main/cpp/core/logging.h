#pragma once

#include <spdlog/logger.h>

namespace mobileclock::core {

// Initializes the process-wide logcat logger. It is safe to call repeatedly.
void initializeLogging();

// Call initializeLogging before using this reference.
spdlog::logger& log();

} // namespace mobileclock::core
