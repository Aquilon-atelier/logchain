#pragma once

#include "context.hpp"
#include "event.hpp"
#include "log_macro.hpp"
#include "target.hpp"

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

    template <typename... Ts> void log_named(std::string_view message, detail::Named<Ts>... named_args) {

        auto ctx = make_context(std::move(named_args)...);

        LogEvent event{.message = message};

        write(event, ctx);
    }

  private:
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
