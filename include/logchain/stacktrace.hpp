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

#include <logchain/log_level.hpp>

#include <exception>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// ===========================================================================
// Stack traces of explicit exceptions (std::stacktrace, C++23)
//
// Opt-in at compile time: nothing below captures or prints anything unless
// LOGCHAIN_WITH_STACKTRACE is defined (the CMake option of the same name
// defines it and links the libstdc++ extension library, stdc++exp).
//
// A stack trace is only useful if it is captured where the exception is
// THROWN: a trace taken in a catch block describes the handler, not the
// failure. This is why the trace is carried by the exception itself:
//
//     throw_traced(std::runtime_error{"boom"});     // captures the trace here
//     ...
//     catch (const std::exception &ex) {
//         LOG_ERROR(logger, "request failed", caused_by(ex));   // prints it
//     }
//
// A trace is rendered only when ALL of these hold:
//   - LOGCHAIN_WITH_STACKTRACE is defined,
//   - the log call receives the exception through caused_by(...),
//   - that exception was thrown through throw_traced(...),
//   - the level of the event is error or fatal,
//   - the renderer option `stacktrace` is true (the default).
//
// Requirements: RTTI (the trace is found with dynamic_cast), and a libstdc++
// built with --enable-libstdcxx-backtrace. The macro must have the same value
// in every translation unit of a program (it changes the layout of traced<E>).
//
// With the feature disabled the API below still compiles and costs nothing:
// traced<E> is E itself, throw_traced() is a plain throw, and renderers never
// print a trace. Code using it needs no #ifdef.
// ===========================================================================

#if defined(LOGCHAIN_WITH_STACKTRACE)
#if __has_include(<stacktrace>)
#include <stacktrace>
#endif
#if !defined(__cpp_lib_stacktrace)
#error "LOGCHAIN_WITH_STACKTRACE is defined but std::stacktrace is not available: use C++23 or later and a "          \
       "libstdc++ configured with --enable-libstdcxx-backtrace (link with -lstdc++exp)."
#endif
#define LOGCHAIN_STACKTRACE_ENABLED 1
#else
#define LOGCHAIN_STACKTRACE_ENABLED 0
#endif

namespace logchain {

// One resolved frame of a stack trace, as printed by the renderers.
struct StackFrame {
    std::string function;
    std::string file;
    unsigned line = 0;
};

namespace detail {

// What caused_by() returns: a non-owning pointer, valid for the duration of
// the log call (the exception outlives the full expression that logs it).
struct CausedBy {
    const std::exception *error;
};

} // namespace detail

// Declares the exception that caused the event being logged. It must be the
// first argument after the message; it is not a format argument:
//
//     LOG_ERROR(logger, "request {} failed", caused_by(ex), request_id);
[[nodiscard]] inline detail::CausedBy caused_by(const std::exception &error) noexcept {
    return detail::CausedBy{&error};
}

#if LOGCHAIN_STACKTRACE_ENABLED

// Carries the stack trace captured when the exception was thrown. Copies of
// the exception (throw, std::exception_ptr...) keep that trace.
class traced_base {
  public:
    explicit traced_base(std::stacktrace trace) noexcept : trace_(std::move(trace)) {}
    traced_base(const traced_base &) = default;
    traced_base &operator=(const traced_base &) = default;
    virtual ~traced_base() = default;

    [[nodiscard]] const std::stacktrace &trace() const noexcept { return trace_; }

  private:
    std::stacktrace trace_;
};

// An exception of type E that also carries a stack trace. It is-a E: existing
// catch (const E &) handlers keep working. It is thrown by throw_traced(); the
// trace is passed explicitly so that it is the caller's, never the one of the
// constructor.
template <typename E> class traced : public E, public traced_base {
    static_assert(std::is_class_v<E> && !std::is_final_v<E> && std::is_base_of_v<std::exception, E>,
                  "traced<E>: E must be a non-final class derived from std::exception.");

  public:
    traced(E error, std::stacktrace captured) : E(std::move(error)), traced_base(std::move(captured)) {}
};

// Throws `error` as a traced<E>, together with the stack trace of the caller.
//
// The trace is a DEFAULT ARGUMENT, so that std::stacktrace::current() is
// evaluated in the caller's expression, like std::source_location::current()
// in event.hpp: its first frame is the function that calls throw_traced(), even
// when the optimizer inlines throw_traced() itself. Do not pass the trace
// explicitly, and do not forward a call through a pack before it reaches here:
// the trace would be that of the forwarding function.
//
// An exception that already carries a trace is thrown as is: the trace of its
// first throw is kept.
template <typename E> [[noreturn]] void throw_traced(E error, std::stacktrace captured = std::stacktrace::current()) {
    if constexpr (std::is_base_of_v<traced_base, E>) {
        throw std::move(error);
    } else {
        throw traced<E>(std::move(error), std::move(captured));
    }
}

#else

template <typename E> using traced = E;

template <typename E> [[noreturn]] void throw_traced(E error) {
    throw std::move(error);
}

#endif

namespace detail {

#if LOGCHAIN_STACKTRACE_ENABLED

// Resolves the frames of `error` when a trace must be printed for an event of
// this level; empty otherwise. This is where the (costly) symbol resolution
// happens: it is never done for an event that prints no trace. Frames whose
// function and file are both unknown are dropped.
inline std::vector<StackFrame> resolve_stack_frames(const std::exception *error, LogLevel level) {
    std::vector<StackFrame> frames;

    if (error == nullptr || level < LogLevel::error) {
        return frames;
    }

    const auto *carrier = dynamic_cast<const traced_base *>(error);

    if (carrier == nullptr) {
        return frames;
    }

    for (const auto &entry : carrier->trace()) {
        std::string function = entry.description();
        std::string file = entry.source_file();

        if (function.empty() && file.empty()) {
            continue;
        }

        frames.push_back(StackFrame{std::move(function), std::move(file), static_cast<unsigned>(entry.source_line())});
    }

    return frames;
}

#else

inline std::vector<StackFrame> resolve_stack_frames(const std::exception *, LogLevel) { return {}; }

#endif

// Frames to print for an event. Events that do not carry an exception (custom
// event types, events built by hand) simply have none.
template <typename Event> std::vector<StackFrame> resolve_event_stack_frames([[maybe_unused]] const Event &event) {
    if constexpr (requires { event.error; }) {
        return resolve_stack_frames(event.error, event.level);
    } else {
        return {};
    }
}

} // namespace detail

} // namespace logchain
