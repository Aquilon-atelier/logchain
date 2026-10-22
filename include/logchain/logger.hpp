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
#include <logchain/event.hpp>
#include <logchain/log_level.hpp>
#include <logchain/log_macro.hpp>
#include <logchain/log_proxy.hpp>
#include <logchain/target.hpp>

#include <atomic>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace logchain {

namespace detail {

template <typename... Ts> struct named_suffix_count;

template <> struct named_suffix_count<> : std::integral_constant<std::size_t, 0> {};

template <typename FirstT, typename... RestT> struct named_suffix_count<FirstT, RestT...> {
  private:
    static constexpr std::size_t rest_count = named_suffix_count<RestT...>::value;

    static constexpr bool rest_is_all_named = rest_count == sizeof...(RestT);

  public:
    static constexpr std::size_t value = rest_is_all_named && is_named_v<FirstT> ? rest_count + 1 : rest_count;
};

template <typename... Args> inline constexpr std::size_t named_count_v = named_suffix_count<Args...>::value;

} // namespace detail

// ===========================================================================
// Logger
//
// Rendering is statically dispatched through Target<RendererT>.
// The only runtime polymorphic call in the pipeline is Sink::write().
//
// Level filtering: is_enabled() is a single relaxed atomic load.
// log() checks the level; log_unchecked() assumes it was already checked
// (used by LogProxy and by the LOG macros).
// ===========================================================================

template <typename... TargetT> class Logger {
  public:
    explicit Logger(TargetT... targets) : targets_(std::move(targets)...) {}

    // std::atomic makes Logger non-copyable and non-movable.
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    // --- Level filtering ----------------------------------------------------

    void set_minimum_level(LogLevel level) noexcept { minimum_level_.store(level, std::memory_order_relaxed); }

    [[nodiscard]] LogLevel minimum_level() const noexcept { return minimum_level_.load(std::memory_order_relaxed); }

    [[nodiscard]] bool is_enabled(LogLevel level) const noexcept { return level >= minimum_level(); }

    // Generic API with an internal level guard. Cannot prevent the caller
    // from evaluating its arguments (use the macros or the proxies for that).
    template <typename... Args> void log(LogLevel level, std::string_view message, Args &&...args) {
        if (is_enabled(level)) {
            log_unchecked(level, message, std::forward<Args>(args)...);
        }
    }

    // Internal primitive: assumes is_enabled(level) is already true.
    // Public because it is called by LogProxy and by the macros.
    template <typename... Args> void log_unchecked(LogLevel level, std::string_view message, Args &&...args) {
        if constexpr (sizeof...(Args) == 0) {
            emit(level, std::string(message), make_context());
        } else {
            dispatch(level, message, std::forward<Args>(args)...);
        }
    }

    // --- Per-level accessors: if (auto log = logger.debug()) log(...); ------

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
    template <typename... Args> static constexpr std::size_t named_count_v = detail::named_count_v<Args...>;

    // -----------------------------------------------------------------------
    // dispatch
    // -----------------------------------------------------------------------

    template <typename... Args> void dispatch(LogLevel level, std::string_view message, Args &&...args) {
        using LastT = std::tuple_element_t<sizeof...(Args) - 1, std::tuple<Args...>>;

        if constexpr (detail::is_extra_v<LastT>) {
            dispatch_with_extra(level, message, std::make_index_sequence<sizeof...(Args) - 1>{},
                                std::forward<Args>(args)...);
        } else if constexpr (named_count_v<Args...> > 0) {
            dispatch_with_named(level, message, std::forward<Args>(args)...);
        } else {
            std::string formatted = std::vformat(message, std::make_format_args(args...));

            emit(level, formatted, make_context());
        }
    }

    // -----------------------------------------------------------------------
    // extra(...) path.
    // -----------------------------------------------------------------------

    template <std::size_t... Is, typename... Args>
    void dispatch_with_extra(LogLevel level, std::string_view message, std::index_sequence<Is...>, Args &&...args) {

        auto args_tuple = std::forward_as_tuple(args...);

        std::string formatted = std::vformat(message, std::make_format_args(std::get<Is>(args_tuple)...));

        auto &&extra_obj = std::get<sizeof...(Args) - 1>(args_tuple);

        auto ctx = make_context_from(extra_obj);

        emit(level, formatted, ctx);
    }

    // -----------------------------------------------------------------------
    // Explicit named(...) path.
    // -----------------------------------------------------------------------

    template <typename... Args> void dispatch_with_named(LogLevel level, std::string_view message, Args &&...args) {

        constexpr std::size_t total = sizeof...(Args);
        constexpr std::size_t named_count = named_count_v<Args...>;
        constexpr std::size_t format_count = total - named_count;

        auto args_tuple = std::forward_as_tuple(std::forward<Args>(args)...);

        dispatch_with_named_impl(level, message, std::move(args_tuple), std::make_index_sequence<format_count>{},
                                 std::make_index_sequence<named_count>{});
    }

    template <typename Tuple, std::size_t... FormatIs, std::size_t... NamedIs>
    void dispatch_with_named_impl(LogLevel level, std::string_view message, Tuple &&args_tuple,
                                  std::index_sequence<FormatIs...>, std::index_sequence<NamedIs...>) {

        constexpr std::size_t format_count = sizeof...(FormatIs);

        std::string formatted = std::vformat(message, std::make_format_args(std::get<FormatIs>(args_tuple)...));

        auto ctx = make_context(std::get<format_count + NamedIs>(args_tuple)...);

        emit(level, formatted, ctx);
    }

    // -----------------------------------------------------------------------
    // Context construction from extra(...).
    // -----------------------------------------------------------------------

    template <typename... Ts> static auto make_context_from(detail::Extra<Ts...> &e) {
        return std::apply([](auto &...named) { return make_context(named...); }, e.items);
    }

    template <typename NodeT> void emit(LogLevel level, const std::string &message, const Context<NodeT> &ctx) {

        LogEvent event{.level = level, .timestamp = std::chrono::system_clock::now(), .message = message};

        write(event, ctx);
    }

    template <typename Event, typename NodeT> void write(const Event &event, const Context<NodeT> &context) {

        std::apply([&](auto &...target) { (target.write(event, context), ...); }, targets_);
    }

    std::tuple<TargetT...> targets_;
    std::atomic<LogLevel> minimum_level_{LogLevel::trace};
};

// CTAD:
//   Logger{Target{JsonRenderer{}, file_sink}, Target{ConsoleRenderer{}, console_sink}}
//   -> Logger<Target<JsonRenderer>, Target<ConsoleRenderer>>

template <typename... TargetT> Logger(TargetT...) -> Logger<TargetT...>;

} // namespace logchain
