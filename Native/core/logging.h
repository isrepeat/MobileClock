#pragma once

#include <spdlog/logger.h>

#include <string_view>

namespace mobileclock::core {
    // Sets the private Android file used for the rotating log before initialization.
    void configureLogFile(std::string_view path);

    // Initializes the process-wide Logcat and rotating-file logger. It is safe to call repeatedly.
    void initializeLogging();

    // Call initializeLogging before using this reference.
    spdlog::logger& log();

    // Flushes the log before Kotlin exports a snapshot to a user-selected file.
    void flushLogging();
} // namespace mobileclock::core

// Передаём source location в spdlog, чтобы Android-лог повторял формат LogHelpers:
// уровень, поток, время, файл, строка, функция и сообщение.
#define LOG(logLevel, ...) \
    ::mobileclock::core::log().log( \
        spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, \
        spdlog::level::logLevel, \
        __VA_ARGS__)

#define LOG_TRACE(...) LOG(trace, __VA_ARGS__)
#define LOG_DEBUG(...) LOG(debug, __VA_ARGS__)
#define LOG_INFO(...) LOG(info, __VA_ARGS__)
#define LOG_WARNING(...) LOG(warn, __VA_ARGS__)
#define LOG_ERROR(...) LOG(err, __VA_ARGS__)