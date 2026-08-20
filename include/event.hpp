#pragma once

#include <string_view>

namespace logchain {

enum class LogLevel { trace, debug, info, warning, error, fatal };

constexpr std::string_view to_string(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::trace:
        return "TRACE";
    case LogLevel::debug:
        return "DEBUG";
    case LogLevel::info:
        return "INFO";
    case LogLevel::warning:
        return "WARN";
    case LogLevel::error:
        return "ERROR";
    case LogLevel::fatal:
        return "FATAL";
    }

    return "UNKNOWN";
}

struct LogEvent {
    LogLevel level;
    std::chrono::system_clock::time_point timestamp;
    std::string_view message;
};

} // namespace logchain
