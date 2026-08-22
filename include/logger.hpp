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
    template <typename... Args> void dispatch(LogLevel level, std::string_view message, Args &&...args) {
        using LastT = std::tuple_element_t<sizeof...(Args) - 1, std::tuple<Args...>>;

        if constexpr (detail::is_extra_v<LastT>) {
            dispatch_with_extra(level, message, std::make_index_sequence<sizeof...(Args) - 1>{},
                                std::forward<Args>(args)...);
        } else {
            std::string formatted = std::vformat(message, std::make_format_args(args...));
            emit(level, formatted, make_context());
        }
    }

    template <std::size_t... Is, typename... Args>
    void dispatch_with_extra(LogLevel level, std::string_view message, std::index_sequence<Is...>, Args &&...args) {
        auto args_tuple = std::forward_as_tuple(args...); // args sont des lvalues ici
        std::string formatted = std::vformat(message, std::make_format_args(std::get<Is>(args_tuple)...));

        auto &&extra_obj = std::get<sizeof...(Args) - 1>(args_tuple);
        auto ctx = make_context_from(extra_obj);
        emit(level, formatted, ctx);
    }

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
