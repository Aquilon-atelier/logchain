// log_proxy.hpp
//
// LogProxy<Level, LoggerT> and the LOGCHAIN_LOG*/short-alias macros.
// Entirely generic over LoggerT (only requires .is_enabled(LogLevel) and
// .log_unchecked(LogLevel, ...) to exist) — no dependency on any concrete
// Logger type. Shared by:
//   - logger.hpp (the real Logger<TargetT...>)
//   - log_level_filtering.hpp (the standalone, non-template reference/
//     test-double Logger used by log_level_filtering_test.cpp to test
//     filtering mechanics in isolation from the full pipeline)
//
// Factored out specifically so those two headers never need to define
// LogProxy/the macros twice, and so log_level_filtering.hpp's standalone
// Logger and logger.hpp's Logger<TargetT...> can each exist without
// colliding on the shared LogProxy/macro machinery.
#pragma once

#include "log_level.hpp"

#include <utility>

namespace logchain {

// ---------------------------------------------------------------------
// LogProxy<Level, LoggerT>
//
// Ne possède pas le logger : `logger_` est une référence. Le proxy est
// pensé pour un usage immédiat et transitoire :
//
//     if (auto log = logger.debug())
//         log("...", ...);
//
// Ne pas le stocker au-delà de la portée dans laquelle `logger` est
// valide (même contrainte qu'un std::string_view vis-à-vis de son
// buffer).
// ---------------------------------------------------------------------
template <LogLevel Level, typename LoggerT> class LogProxy {
  public:
    explicit LogProxy(LoggerT &logger) noexcept : logger_(logger) {}

    // Un seul load atomique (relaxed, via is_enabled()).
    [[nodiscard]] explicit operator bool() const noexcept { return logger_.is_enabled(Level); }

    // N'est censé être appelé que si operator bool() vient de renvoyer
    // true — aucun test de niveau ici, c'est log_unchecked() en aval qui
    // en dépend implicitement.
    template <typename... Args> void operator()(Args &&...args) {
        logger_.log_unchecked(Level, std::forward<Args>(args)...);
    }

  private:
    LoggerT &logger_;
};

} // namespace logchain

// ---------------------------------------------------------------------
// Macros
//
// LOGCHAIN_LOG capture `logger_expr` une seule fois dans une variable
// locale (do/while(0) + auto&&), donc sûre même si l'expression a un
// effet de bord (ex: LOGCHAIN_LOG(get_logger(), ...)).
//
// Les macros courtes (LOG, LOG_DEBUG, ...) sont fournies par défaut mais
// désactivables via LOGCHAIN_NO_SHORT_MACROS si elles entrent en
// collision avec une autre lib (glog, Abseil, ...).
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
