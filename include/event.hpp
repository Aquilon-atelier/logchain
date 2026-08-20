// Copyright (c) 2026 Sylvain Ladoux
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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
