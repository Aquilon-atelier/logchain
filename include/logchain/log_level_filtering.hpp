// log_level_filtering.hpp
//
// Implémentation de référence pour la spec "Filtrage de niveau de log et
// dispatch précoce" (logchain).
//
// Ce fichier est volontairement autonome et ne dépend pas du reste de
// logchain : `log_unchecked()` est un stub minimal qui imprime le message.
// Dans la vraie lib, `log_unchecked()` reste le pipeline existant
// (dispatch -> named/extra -> vformat -> make_context -> emit -> Target)
// et `Logger` devient `Logger<TargetT...>` sans que rien ici ne change —
// LogProxy est déduit depuis le type englobant, pas depuis TargetT....
//
// Corrections intégrées par rapport au sketch initial de la spec :
//   1. La macro LOG() ne capture l'expression `logger` qu'une seule fois
//      (évite une double évaluation si l'appelant passe une expression
//      avec effet de bord, ex: LOG(get_logger(), ...)).
//   2. LogProxy::operator bool() est `explicit`.
//   3. Logger::trace()/debug()/.../log() sont `[[nodiscard]]` pour éviter
//      un `logger.debug();` silencieusement inutile.
//   4. LogProxy::operator() forwarde parfaitement ses arguments.
//   5. Durée de vie de LogProxy::logger_ documentée (référence, pas de
//      stockage au-delà de l'expression `if`).
//
#pragma once

#include <atomic>
#include <cstdio>
#include <string>
#include <utility>

#include <logchain/log_level.hpp>

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

// ---------------------------------------------------------------------
// Logger — squelette minimal pour la démo. Dans la vraie lib, c'est
// Logger<TargetT...> ; seuls minimum_level_/is_enabled/log/log_unchecked
// et les accesseurs trace()/debug()/... nous intéressent ici.
// ---------------------------------------------------------------------
class Logger {
  public:
    Logger() noexcept : minimum_level_(LogLevel::trace) {}

    void set_minimum_level(LogLevel level) noexcept { minimum_level_.store(level, std::memory_order_relaxed); }

    [[nodiscard]] LogLevel minimum_level() const noexcept { return minimum_level_.load(std::memory_order_relaxed); }

    [[nodiscard]] bool is_enabled(LogLevel level) const noexcept {
        return level >= minimum_level_.load(std::memory_order_relaxed);
    }

    // API générique : garde interne obligatoire. Ne peut pas empêcher
    // l'évaluation préalable des arguments par l'appelant (limitation du
    // C++, pas de l'implémentation).
    template <typename... Args> void log(LogLevel level, Args &&...args) {
        if (is_enabled(level)) {
            log_unchecked(level, std::forward<Args>(args)...);
        }
    }

    // Primitive interne : suppose is_enabled(level) déjà vrai, ne
    // refait jamais le test.
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
