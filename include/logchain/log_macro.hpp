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

#include <logchain/context.hpp>
#include <logchain/log_level.hpp>

#define SCL_PARENS ()

#define SCL_EXPAND(...) SCL_EXPAND4(SCL_EXPAND4(SCL_EXPAND4(SCL_EXPAND4(__VA_ARGS__))))
#define SCL_EXPAND4(...) SCL_EXPAND3(SCL_EXPAND3(SCL_EXPAND3(SCL_EXPAND3(__VA_ARGS__))))
#define SCL_EXPAND3(...) SCL_EXPAND2(SCL_EXPAND2(SCL_EXPAND2(SCL_EXPAND2(__VA_ARGS__))))
#define SCL_EXPAND2(...) SCL_EXPAND1(SCL_EXPAND1(SCL_EXPAND1(SCL_EXPAND1(__VA_ARGS__))))
#define SCL_EXPAND1(...) __VA_ARGS__

#define SCL_FOR_EACH(macro, ...) __VA_OPT__(SCL_EXPAND(SCL_FOR_EACH_HELPER(macro, __VA_ARGS__)))
#define SCL_FOR_EACH_HELPER(macro, a1, ...) macro(a1) __VA_OPT__(, SCL_FOR_EACH_AGAIN SCL_PARENS(macro, __VA_ARGS__))
#define SCL_FOR_EACH_AGAIN() SCL_FOR_EACH_HELPER

#define SCL_NAMED_ARG(x) ::logchain::detail::make_named(#x, (x))

// extra(user, request_id) -> make_extra(make_named("user", user), ...)
#define LOGCHAIN_EXTRA(...) ::logchain::detail::make_extra(SCL_FOR_EACH(SCL_NAMED_ARG, __VA_ARGS__))

// ---------------------------------------------------------------------------
// LOGCHAIN_LOG : captures `logger_expr` exactly once, tests the level BEFORE
// evaluating the message arguments (they are only evaluated when the level is
// enabled), then calls log_unchecked().
//
// `logger_expr` is bound once to a local reference (do/while(0) + auto&&), so
// the macro is safe even when the expression has side effects, e.g.
// LOGCHAIN_LOG(get_logger(), ...).
// ---------------------------------------------------------------------------
#define LOGCHAIN_LOG(logger_expr, level, message, ...)                                                                 \
    do {                                                                                                               \
        auto &&logchain_logger_ = (logger_expr);                                                                       \
        if (logchain_logger_.is_enabled(level)) {                                                                      \
            logchain_logger_.log_unchecked((level), (message)__VA_OPT__(, ) __VA_ARGS__);                              \
        }                                                                                                              \
    } while (0)

#define LOGCHAIN_LOG_TRACE(logger, message, ...)                                                                       \
    LOGCHAIN_LOG(logger, ::logchain::LogLevel::trace, message __VA_OPT__(, ) __VA_ARGS__)
#define LOGCHAIN_LOG_DEBUG(logger, message, ...)                                                                       \
    LOGCHAIN_LOG(logger, ::logchain::LogLevel::debug, message __VA_OPT__(, ) __VA_ARGS__)
#define LOGCHAIN_LOG_INFO(logger, message, ...)                                                                        \
    LOGCHAIN_LOG(logger, ::logchain::LogLevel::info, message __VA_OPT__(, ) __VA_ARGS__)
#define LOGCHAIN_LOG_WARN(logger, message, ...)                                                                        \
    LOGCHAIN_LOG(logger, ::logchain::LogLevel::warning, message __VA_OPT__(, ) __VA_ARGS__)
#define LOGCHAIN_LOG_ERROR(logger, message, ...)                                                                       \
    LOGCHAIN_LOG(logger, ::logchain::LogLevel::error, message __VA_OPT__(, ) __VA_ARGS__)
#define LOGCHAIN_LOG_FATAL(logger, message, ...)                                                                       \
    LOGCHAIN_LOG(logger, ::logchain::LogLevel::fatal, message __VA_OPT__(, ) __VA_ARGS__)

// ---------------------------------------------------------------------------
// Short macros (extra, LOG, LOG_DEBUG, ...) are provided by default. Define
// LOGCHAIN_NO_SHORT_MACROS before including logchain to disable them when
// they collide with another library (glog, Abseil, ...); the LOGCHAIN_*
// forms remain available.
// ---------------------------------------------------------------------------
#ifndef LOGCHAIN_NO_SHORT_MACROS
#define extra LOGCHAIN_EXTRA
#define LOG LOGCHAIN_LOG
#define LOG_TRACE LOGCHAIN_LOG_TRACE
#define LOG_DEBUG LOGCHAIN_LOG_DEBUG
#define LOG_INFO LOGCHAIN_LOG_INFO
#define LOG_WARN LOGCHAIN_LOG_WARN
#define LOG_ERROR LOGCHAIN_LOG_ERROR
#define LOG_FATAL LOGCHAIN_LOG_FATAL
#endif
