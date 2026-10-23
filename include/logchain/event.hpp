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

#include <chrono>
#include <concepts>
#include <optional>
#include <source_location>
#include <string_view>

#include <logchain/log_level.hpp>
namespace logchain {

// ===========================================================================
// Call-site information (std::source_location, C++20: no compiler extension)
//
// SourceMode selects what a renderer prints about the call site:
//   Never : nothing (default: renderer output is unchanged)
//   Short : file basename + line
//   Full  : file path as given to the compiler + line + function name
// ===========================================================================
enum class SourceMode { Never, Short, Full };

namespace detail {
constexpr std::string_view source_basename(std::string_view path) noexcept {
    const auto pos = path.find_last_of("/\\");
    return pos == std::string_view::npos ? path : path.substr(pos + 1);
}
} // namespace detail

// Message + call site. Used as the message parameter of Logger::log(): the
// default argument is evaluated in the caller's call expression, so the macros
// of log_macro.hpp (which expand in the caller's function) need no change.
// Do NOT forward a message through a pack (Args&&...) before it reaches a
// Located parameter: the location would then be that of the forwarding function.
struct Located {
    std::string_view message;
    std::source_location loc;

    // Template constructor: a string literal converts to Located in ONE step
    // (const char* -> string_view -> Located would need two user conversions).
    template <typename S>
        requires std::convertible_to<const S &, std::string_view>
    constexpr Located(const S &m, std::source_location l = std::source_location::current()) noexcept
        : message(m), loc(l) {}
};

// Carried through the dispatch pipeline in place of the bare LogLevel.
struct CallInfo {
    LogLevel level;
    std::source_location loc;
};

struct LogEvent {
    LogLevel level;
    std::chrono::system_clock::time_point timestamp;
    std::string_view message;
    // Call site of the LOG statement; empty for events built without one.
    std::optional<std::source_location> where = std::nullopt;
};

} // namespace logchain
