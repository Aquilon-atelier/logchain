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

#include "context.hpp"
#include "event.hpp"
#include "log_macro.hpp"
#include "target.hpp"

#include <chrono>
#include <concepts>
#include <tuple>
#include <utility>

namespace logchain {

// ===========================================================================
// named_suffix_count
//
// Number of trailing Named<T> arguments.
//
// Examples:
//
//   int
//       -> 0
//
//   int, Named<A>
//       -> 1
//
//   int, Named<A>, Named<B>
//       -> 2
//
// Named arguments are therefore required to form a suffix.
//
// NOTE: this must live at namespace scope, not as a member of Logger.
// A member template of a class template cannot be explicitly or partially
// specialized while still nested inside its enclosing class template —
// GCC rejects it with "explicit specialization in non-namespace scope".
// Since named_suffix_count never actually depends on Logger's TargetT...
// pack, hoisting it out here is free.
// ===========================================================================
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
//
// The only runtime polymorphic call in the complete pipeline is:
//     Sink::write(std::string_view)
//
// Logger itself does not perform virtual dispatch.
// ===========================================================================

template <typename... TargetT> class Logger {
  public:
    explicit Logger(TargetT... targets) : targets_(std::move(targets)...) {}

    template <typename... Args> void log(LogLevel level, std::string_view message, Args &&...args) {
        if constexpr (sizeof...(Args) == 0) {
            emit(level, std::string(message), make_context());
        } else {
            dispatch(level, message, std::forward<Args>(args)...);
        }
    }

  private:
    // Thin alias so the rest of the class can keep writing named_count_v<Args...>
    // unchanged; the actual metafunction now lives in logchain::detail above.
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
    // Existing extra(...) path.
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
    //
    // Example:
    //
    //   log(level,
    //       "request_handler {}",
    //       handler,
    //       named("Albert Londre", albert_londre),
    //       named("Romain Garry", romain_garry));
    //
    // The format arguments are the prefix and the Named<T> arguments form
    // the context suffix.
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

    // NOTE: 'Event' was previously an unused template parameter here — it
    // never appeared in the function's parameter list, so it could never be
    // deduced from a call site ("couldn't deduce template parameter Event").
    // LogEvent is constructed directly in the body, so only NodeT is needed.
    template <typename NodeT> void emit(LogLevel level, const std::string &message, const Context<NodeT> &ctx) {

        LogEvent event{.level = level, .timestamp = std::chrono::system_clock::now(), .message = message};

        write(event, ctx);
    }

    template <typename Event, typename NodeT> void write(const Event &event, const Context<NodeT> &context) {

        std::apply([&](auto &...target) { (target.write(event, context), ...); }, targets_);
    }

    std::tuple<TargetT...> targets_;
};

// CTAD:
//
// Logger{Target{JsonRenderer{}, file_sink},
//        Target{ConsoleRenderer{}, console_sink}}
//
// -> Logger<Target<JsonRenderer>, Target<ConsoleRenderer>>
//

template <typename... TargetT> Logger(TargetT...) -> Logger<TargetT...>;

} // namespace logchain
