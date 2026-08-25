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

#include "buffering.hpp"
#include "context.hpp"
#include "event.hpp"
#include "log_macro.hpp"
#include "log_proxy.hpp"
#include "size.hpp"
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
    static constexpr std::size_t kDefaultBufferCapacity = 64 * KiB;

    // Existing call shape, left untouched: buffering stays disabled,
    // exactly the pre-feature default behavior. Any code that already
    // does `Logger logger{Target{...}, Target{...}};` keeps compiling
    // and keeps behaving identically.
    explicit Logger(TargetT... targets)
        : Logger(BufferingPolicy::disabled, kDefaultBufferCapacity, std::move(targets)...) {}

    // New: explicit opt-in at construction time, fixed for the logger's
    // entire lifetime. There is no set_buffering()/set_buffer_capacity()
    // setter — that was a deliberate simplification: buffering is an
    // immutable, construction-time property of a Logger, never toggled
    // afterward. This is also what makes it safe to publish a Logger to
    // several threads without any further synchronization on the
    // buffering configuration itself (only the per-thread buffer
    // contents need thread-local isolation; the configuration never
    // changes after construction, so it needs none).
    Logger(BufferingPolicy policy, std::size_t capacity, TargetT... targets)
        : state_(std::make_shared<LoggerState<TargetT...>>(detail::next_logger_id(), policy, capacity,
                                                           std::tuple<TargetT...>(std::move(targets)...))) {}

    Logger(const Logger &) = default;
    Logger &operator=(const Logger &) = default;
    Logger(Logger &&) noexcept = default;
    Logger &operator=(Logger &&) noexcept = default;

    // Best-effort promptness, not a safety requirement: even without this
    // destructor doing anything, buffered events for this logger remain
    // perfectly valid (LoggerState is kept alive by the TLS entry's
    // `strong` pointer) until they are flushed explicitly, flushed here,
    // or flushed at thread exit. Flushing here simply avoids waiting on
    // either of those for the common case of a logger destroyed on the
    // same thread that was using it.
    ~Logger() {
        if (!state_)
            return; // moved-from
        if (auto *tls = find_thread_local_entry()) {
            if (!tls->empty()) {
                try {
                    flush_buffer(*tls);
                } catch (...) {
                    // A destructor must not throw. Explicit flush() is
                    // the API that propagates pipeline exceptions; a
                    // flush incidental to destruction is not. Remaining
                    // events are discarded along with the stale entry.
                }
            }
        }
    }

    template <typename... Args> void log(LogLevel level, std::string_view message, Args &&...args) {
        if (is_enabled(level)) {
            log_unchecked(level, message, std::forward<Args>(args)...);
        }
    }

    // Internal primitive: assumes is_enabled(level) has already returned
    // true and never re-checks. This is exactly the pre-filtering
    // dispatch pipeline (dispatch -> named/extra -> vformat ->
    // make_context -> emit -> buffering policy -> Target), unchanged —
    // only its name and the fact that it's no longer the sole entry
    // point are new. Called directly by LogProxy::operator() and by the
    // LOGCHAIN_LOG* macros (log_level_filtering.hpp), both of which
    // perform their own is_enabled() check beforehand, at the call site,
    // before the message/args are even evaluated — something the
    // checked log() above cannot do, since C++ evaluates its arguments
    // eagerly regardless of what happens inside the function body.
    template <typename... Args> void log_unchecked(LogLevel level, std::string_view message, Args &&...args) {
        if constexpr (sizeof...(Args) == 0) {
            emit(level, std::string(message), make_context());
        } else {
            dispatch(level, message, std::forward<Args>(args)...);
        }
    }

    [[nodiscard]] bool is_enabled(LogLevel level) const noexcept {
        return level >= state_->minimum_level.load(std::memory_order_relaxed);
    }

    [[nodiscard]] LogLevel minimum_level() const noexcept {
        return state_->minimum_level.load(std::memory_order_relaxed);
    }

    void set_minimum_level(LogLevel level) noexcept { state_->minimum_level.store(level, std::memory_order_relaxed); }

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

    // Flushes the buffer belonging to the calling thread and this logger.
    // No-op if buffering is disabled, or if this thread has no buffered
    // events. Propagates any exception raised by the pipeline while
    // emitting — unlike the implicit flushes performed by ~Logger() and
    // thread termination.
    void flush() {
        if (state_->policy == BufferingPolicy::disabled)
            return;
        if (auto *tls = find_thread_local_entry()) {
            if (!tls->empty()) {
                flush_buffer(*tls);
            }
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

            emit(level, std::move(formatted), make_context());
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

        emit(level, std::move(formatted), ctx);
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

        emit(level, std::move(formatted), ctx);
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
    //
    // BUFFERING CHANGE: `message` is now taken by rvalue reference rather
    // than `const std::string &`. This is what lets the buffered path
    // move the already-formatted string into BufferedEvent::message_storage
    // instead of formatting or copying it a second time. The disabled
    // path's behavior is unchanged: LogEvent::message still views the
    // caller's string object for the duration of the synchronous call
    // chain, exactly as before.
    template <typename NodeT> void emit(LogLevel level, std::string &&message, const Context<NodeT> &ctx) {

        if (state_->policy == BufferingPolicy::disabled) {
            LogEvent event{.level = level, .timestamp = std::chrono::system_clock::now(), .message = message};
            state_->emit_now(event, ctx);
            return;
        }

        buffer_or_emit(level, std::chrono::system_clock::now(), std::move(message), ctx);
    }

    // ---------------------------------------------------------------
    // buffer_or_emit(): the capacity algorithm.
    //
    //   if event_size > capacity:
    //       if buffer not empty: flush()
    //       emit event directly            // never inserted
    //   else:
    //       if current_size + event_size > capacity: flush()
    //       append event to buffer
    //
    // The prospective event_size is computed without mutating any
    // committed accounting: the newly-touched context nodes are first
    // *peeked*, the flush decision is made, and only then are the
    // refcount increments committed.
    // ---------------------------------------------------------------
    template <typename NodeT>
    void buffer_or_emit(LogLevel level, std::chrono::system_clock::time_point ts, std::string &&formatted,
                        const Context<NodeT> &ctx) {

        auto &tls = thread_local_entry();
        const auto &leaf = ctx.underlying_node();

        std::vector<std::pair<const void *, std::size_t>> newly_touched;
        std::size_t new_context_bytes = 0;
        walk_context_chain(leaf, [&](const void *id, std::size_t sz) {
            if (!tls.context_refs.contains(id)) {
                new_context_bytes += sz;
                newly_touched.emplace_back(id, sz);
            }
        });

        const std::size_t event_size = formatted.size() + new_context_bytes;

        if (event_size > state_->capacity) {
            // Oversized event: never buffered.
            if (!tls.empty()) {
                flush_buffer(tls);
            }
            LogEvent event{.level = level, .timestamp = ts, .message = formatted};
            state_->emit_now(event, ctx);
            return;
        }

        if (tls.buffered_size + event_size > state_->capacity) {
            flush_buffer(tls);
            // Post-flush, tls.context_refs is empty: every node in the
            // chain is now newly touched, which may include nodes the
            // pre-flush peek above had already excluded because the
            // (now-flushed) buffer used to retain them. Recompute rather
            // than reuse the stale list, or buffered_size would
            // under-count after every capacity-triggered flush.
            new_context_bytes = 0;
            newly_touched.clear();
            walk_context_chain(leaf, [&](const void *id, std::size_t sz) {
                new_context_bytes += sz;
                newly_touched.emplace_back(id, sz);
            });
        }

        for (auto [id, sz] : newly_touched) {
            tls.context_refs.acquire(id, sz);
        }
        tls.buffered_size += formatted.size() + new_context_bytes;

        auto holder = std::make_unique<ContextHolder<NodeT, TargetT...>>(leaf);
        tls.events.emplace_back(std::move(formatted), level, ts, std::move(holder));

        // Strong ownership is acquired lazily, exactly when the buffer
        // transitions from empty to non-empty.
        if (!tls.strong) {
            tls.strong = state_;
        }
    }

    // ---------------------------------------------------------------
    // flush_buffer(): prefix-commit semantics.
    //
    // Events are emitted one at a time, in order. Each event is only
    // removed from the buffer — and its accounting released — after its
    // own emission has succeeded. If the pipeline throws partway
    // through, the events already emitted are gone (never re-emitted);
    // the remaining ones stay buffered and the exception propagates to
    // the caller (explicit flush(), or a capacity-triggered flush
    // initiated by a subsequent log call).
    // ---------------------------------------------------------------
    void flush_buffer(ThreadLocalBuffer<TargetT...> &tls) {
        while (!tls.events.empty()) {
            auto &buffered = tls.events.front();

            buffered.context->emit_via(*state_, buffered.event); // may throw

            // Reached only on successful emission: release this event's
            // contribution before popping, so buffered_size and
            // context_refs are never transiently inconsistent with the
            // buffer's actual contents.
            tls.buffered_size -= buffered.event.message.size();
            buffered.context->release(tls.context_refs, tls.buffered_size);
            tls.events.pop_front();
        }
        if (tls.events.empty()) {
            tls.strong.reset(); // release once truly empty
        }
    }

    // ---------------------------------------------------------------
    // TLS access.
    //
    // thread_local_entry(): auto-creating, used on the log-call hot
    // path. Lazily purges stale entries from OTHER, now-destroyed
    // loggers — but only when a new entry is actually being inserted
    // (amortized over insertions, never on every call to an
    // already-registered logger's entry), keeping the purge off the hot
    // path for the common case of a small, stable set of loggers used
    // repeatedly by a thread.
    //
    // find_thread_local_entry(): non-creating, used by flush() and
    // ~Logger() — neither should conjure a buffer into existence for a
    // thread that never logged anything through this logger.
    // ---------------------------------------------------------------
    ThreadLocalBuffer<TargetT...> &thread_local_entry() {
        auto [it, inserted] = t_buffers_.try_emplace(state_->id);
        if (inserted) {
            it->second.weak = state_;
            purge_stale_entries();
        }
        return it->second;
    }

    ThreadLocalBuffer<TargetT...> *find_thread_local_entry() {
        auto it = t_buffers_.find(state_->id);
        return it == t_buffers_.end() ? nullptr : &it->second;
    }

    void purge_stale_entries() {
        for (auto it = t_buffers_.begin(); it != t_buffers_.end();) {
            if (it->second.empty() && it->second.weak.expired()) {
                it = t_buffers_.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::shared_ptr<LoggerState<TargetT...>> state_;

    // One TLS map per Logger<TargetT...> instantiation. LoggerId is
    // globally unique (see detail::next_logger_id()), so multiple
    // Logger<TargetT...> objects of the SAME instantiation correctly
    // share one map keyed by id, while different TargetT... combinations
    // simply get entirely separate maps — no cross-instantiation type
    // erasure is needed at this layer.
    static thread_local std::unordered_map<LoggerId, ThreadLocalBuffer<TargetT...>> t_buffers_;
};

template <typename... TargetT>
thread_local std::unordered_map<LoggerId, ThreadLocalBuffer<TargetT...>> Logger<TargetT...>::t_buffers_;

// CTAD:
//
// Logger{Target{JsonRenderer{}, file_sink},
//        Target{ConsoleRenderer{}, console_sink}}
//
// -> Logger<Target<JsonRenderer>, Target<ConsoleRenderer>>
//
// The second guide covers the new leading-BufferingPolicy overload:
//
// Logger{BufferingPolicy::local, 64 * KiB,
//        Target{JsonRenderer{}, file_sink}}
//

template <typename... TargetT> Logger(TargetT...) -> Logger<TargetT...>;
template <typename... TargetT> Logger(BufferingPolicy, std::size_t, TargetT...) -> Logger<TargetT...>;

} // namespace logchain
