#pragma once

#include "context.hpp"

namespace logchain {

#define SCL_PARENS ()

#define SCL_EXPAND(...) SCL_EXPAND4(SCL_EXPAND4(SCL_EXPAND4(SCL_EXPAND4(__VA_ARGS__))))
#define SCL_EXPAND4(...) SCL_EXPAND3(SCL_EXPAND3(SCL_EXPAND3(SCL_EXPAND3(__VA_ARGS__))))
#define SCL_EXPAND3(...) SCL_EXPAND2(SCL_EXPAND2(SCL_EXPAND2(SCL_EXPAND2(__VA_ARGS__))))
#define SCL_EXPAND2(...) SCL_EXPAND1(SCL_EXPAND1(SCL_EXPAND1(SCL_EXPAND1(__VA_ARGS__))))
#define SCL_EXPAND1(...) __VA_ARGS__

#define SCL_FOR_EACH(macro, ...) __VA_OPT__(SCL_EXPAND(SCL_FOR_EACH_HELPER(macro, __VA_ARGS__)))

#define SCL_FOR_EACH_HELPER(macro, a1, ...) macro(a1) __VA_OPT__(, SCL_FOR_EACH_AGAIN SCL_PARENS(macro, __VA_ARGS__))

#define SCL_FOR_EACH_AGAIN() SCL_FOR_EACH_HELPER

#define SCL_NAMED_ARG(x) logchain::detail::make_named(#x, (x))

#define LOG(logger, level, message, ...)                                                                               \
    (logger).log_named((level), (message)__VA_OPT__(, SCL_FOR_EACH(SCL_NAMED_ARG, __VA_ARGS__)))

#define LOG_TRACE(logger, message, ...) LOG(logger, ::logchain::LogLevel::trace, message __VA_OPT__(, ) __VA_ARGS__)

#define LOG_DEBUG(logger, message, ...) LOG(logger, ::logchain::LogLevel::debug, message __VA_OPT__(, ) __VA_ARGS__)

#define LOG_INFO(logger, message, ...) LOG(logger, ::logchain::LogLevel::info, message __VA_OPT__(, ) __VA_ARGS__)

#define LOG_WARN(logger, message, ...) LOG(logger, ::logchain::LogLevel::warning, message __VA_OPT__(, ) __VA_ARGS__)

#define LOG_ERROR(logger, message, ...) LOG(logger, ::logchain::LogLevel::error, message __VA_OPT__(, ) __VA_ARGS__)

#define LOG_FATAL(logger, message, ...) LOG(logger, ::logchain::LogLevel::fatal, message __VA_OPT__(, ) __VA_ARGS__)

} // namespace logchain
