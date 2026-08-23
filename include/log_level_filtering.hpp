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

//
// Reference implementation for the "Log level filtering and
// early dispatch" spec (logchain).
//
// This file is intentionally self-contained and does not depend on the rest of
// logchain: `log_unchecked()` is a minimal stub that prints the message.
// In the actual library, `log_unchecked()` remains the existing pipeline
// (dispatch -> named/extra -> vformat -> make_context -> emit -> Target)
// and `Logger` becomes `Logger<TargetT...>` without anything here changing—
// LogProxy is deduced from the enclosing type, not from TargetT....
//
// Corrections incorporated relative to the initial spec sketch:
//   1. The LOG() macro captures the `logger` expression only once
//      (avoids double evaluation if the caller passes an expression
//      with side effects, e.g., LOG(get_logger(), ...)).
//   2. LogProxy::operator bool() is `explicit`.
//   3. Logger::trace()/debug()/.../log() are `[[nodiscard]]` to avoid
//      a silently useless `logger.debug();` call.
//   4. LogProxy::operator() perfectly forwards its arguments.
//   5. LogProxy::logger_ lifetime documented (reference, no
//      storage beyond the `if` expression).
//

#pragma once

#include <atomic>
#include <cstdio>
#include <string>
#include <utility>

#include "log_level.hpp"

namespace logchain {

// ---------------------------------------------------------------------
// LogProxy<Level, LoggerT>
//
// Does not own the logger: `logger_` is a reference. The proxy is
// designed for immediate, transient use:
//
//     if (auto log = logger.debug())
//         log("...", ...);
//
// Do not store it beyond the scope in which `logger` is
// valid (same constraint as `std::string_view` regarding its
// buffer).
// ---------------------------------------------------------------------
template <LogLevel Level, typename LoggerT> class LogProxy {
  public:
    explicit LogProxy(LoggerT &logger) noexcept : logger_(logger) {}

    // A single atomic load (relaxed, via is_enabled()).
    [[nodiscard]] explicit operator bool() const noexcept { return logger_.is_enabled(Level); }

    // Should only be called if operator bool() has just returned
    // true — no level check here; it is log_unchecked() further down the
    // line that implicitly relies on this.
    template <typename... Args> void operator()(Args &&...args) {
        logger_.log_unchecked(Level, std::forward<Args>(args)...);
    }

  private:
    LoggerT &logger_;
};

class Logger {
  public:
    Logger() noexcept : minimum_level_(LogLevel::trace) {}

    void set_minimum_level(LogLevel level) noexcept { minimum_level_.store(level, std::memory_order_relaxed); }

    [[nodiscard]] LogLevel minimum_level() const noexcept { return minimum_level_.load(std::memory_order_relaxed); }

    [[nodiscard]] bool is_enabled(LogLevel level) const noexcept {
        return level >= minimum_level_.load(std::memory_order_relaxed);
    }

    // Generic API: internal guard required. Cannot prevent
    // the caller from evaluating arguments beforehand (a C++
    // limitation, not an implementation one).
    template <typename... Args> void log(LogLevel level, Args &&...args) {
        if (is_enabled(level)) {
            log_unchecked(level, std::forward<Args>(args)...);
        }
    }

    // Internal primitive: assumes is_enabled(level) is already true;
    // never re-runs the test.
    template <typename... Args> void log_unchecked(LogLevel level, const std::string &message, Args &&.../*args*/) {

        const auto level_name = to_string(level);

        std::printf("[%.*s] %s\n", static_cast<int>(level_name.size()), level_name.data(), message.c_str());
    }

    [[nodiscard]] LogProxy<LogLevel::trace, Logger> trace() noexcept {
        return LogProxy<LogLevel::trace, Logger>(*this);
    }
    [[nodiscard]] LogProxy<LogLevel::debug, Logger> debug() noexcept {
        return LogProxy<LogLevel::debug, Logger>(*this);
    }
    [[nodiscard]] LogProxy<LogLevel::info, Logger> info() noexcept { return LogProxy<LogLevel::info, Logger>(*this); }
    [[nodiscard]] LogProxy<LogLevel::warning, Logger> warning() noexcept {
        return LogProxy<LogLevel::warning, Logger>(*this);
    }
    [[nodiscard]] LogProxy<LogLevel::error, Logger> error() noexcept {
        return LogProxy<LogLevel::error, Logger>(*this);
    }
    [[nodiscard]] LogProxy<LogLevel::fatal, Logger> fatal() noexcept {
        return LogProxy<LogLevel::fatal, Logger>(*this);
    }

  private:
    std::atomic<LogLevel> minimum_level_;
};

} // namespace logchain

// ---------------------------------------------------------------------
// Macros
//
// LOGCHAIN_LOG captures `logger_expr` exactly once in a local variable
// (using do/while(0) + auto&&), making it safe even if the expression
// has side effects (e.g., LOGCHAIN_LOG(get_logger(), ...)).
//
// Short macros (LOG, LOG_DEBUG, ...) are provided by default but can be
// disabled via LOGCHAIN_NO_SHORT_MACROS if they conflict with another
// library (glog, Abseil, ...).
// ---------------------------------------------------------------------
#define LOGCHAIN_LOG(logger_expr, level, ...)                                                                          \
    do {                                                                                                               \
        auto &&logchain_logger_ = (logger_expr);                                                                       \
        if (logchain_logger_.is_enabled(level)) {                                                                      \
            logchain_logger_.log_unchecked(level __VA_OPT__(, ) __VA_ARGS__);                                          \
        }                                                                                                              \
    } while (0)

#define LOGCHAIN_LOG_TRACE(logger_expr, ...) LOGCHAIN_LOG(logger_expr, ::logchain::LogLevel::trace, __VA_ARGS__)
#define LOGCHAIN_LOG_DEBUG(logger_expr, ...) LOGCHAIN_LOG(logger_expr, ::logchain::LogLevel::debug, __VA_ARGS__)
#define LOGCHAIN_LOG_INFO(logger_expr, ...) LOGCHAIN_LOG(logger_expr, ::logchain::LogLevel::info, __VA_ARGS__)
#define LOGCHAIN_LOG_WARN(logger_expr, ...) LOGCHAIN_LOG(logger_expr, ::logchain::LogLevel::warning, __VA_ARGS__)
#define LOGCHAIN_LOG_ERROR(logger_expr, ...) LOGCHAIN_LOG(logger_expr, ::logchain::LogLevel::error, __VA_ARGS__)
#define LOGCHAIN_LOG_FATAL(logger_expr, ...) LOGCHAIN_LOG(logger_expr, ::logchain::LogLevel::fatal, __VA_ARGS__)

#ifndef LOGCHAIN_NO_SHORT_MACROS
#define LOG LOGCHAIN_LOG
#define LOG_TRACE LOGCHAIN_LOG_TRACE
#define LOG_DEBUG LOGCHAIN_LOG_DEBUG
#define LOG_INFO LOGCHAIN_LOG_INFO
#define LOG_WARN LOGCHAIN_LOG_WARN
#define LOG_ERROR LOGCHAIN_LOG_ERROR
#define LOG_FATAL LOGCHAIN_LOG_FATAL
#endif
