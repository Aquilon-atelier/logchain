// buffering.hpp
//
// Buffering policy for logchain (spec: "Buffering policy de logchain", V2
// + the immutable-configuration revision agreed afterward).
//
// This file is additive: it does not modify context.hpp, event.hpp,
// target.hpp, or sink.hpp. It is included by logger.hpp, which is where
// the actual wiring happens (a handful of targeted edits — see the
// comments in logger.hpp itself).
#pragma once

#include "context.hpp"
#include "event.hpp"
#include "log_level.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace logchain {

enum class BufferingPolicy {
    disabled,
    local,
};

using LoggerId = std::uint64_t;

namespace detail {

// A single counter shared by every Logger<TargetT...> instantiation
// (function-local static — thread-safe init via magic statics, no ODR
// concerns in a header-only library). IDs are unique process-wide, which
// is what lets a per-instantiation TLS map (see Logger::t_buffers_) use a
// flat LoggerId key with no risk of cross-instantiation collision.
inline LoggerId next_logger_id() {
    static std::atomic<LoggerId> counter{1};
    return counter.fetch_add(1, std::memory_order_relaxed);
}

} // namespace detail

// ===========================================================================
// ContextRefCounts
//
// Tracks, per local buffer, how many currently-buffered events retain
// each distinct ContextNode. A node contributes to buffered_size exactly
// once regardless of how many buffered events reference it, and that
// contribution is released precisely when the LAST referencing event
// leaves the buffer — whether via a full flush, a partial (prefix-commit)
// flush, or a capacity-triggered flush.
//
// A plain "seen" set cannot express release: nothing would ever un-mark
// an id when its owning event is removed, so buffered_size would drift
// upward forever across repeated partial flushes. Hence a refcount map.
// ===========================================================================
class ContextRefCounts {
  public:
    // Non-mutating peek, used to compute a *prospective* event size before
    // committing to an insertion (capacity decisions must not mutate
    // state prematurely).
    bool contains(const void *node_id) const { return counts_.contains(node_id); }

    // Registers one more buffered event as referencing `node_id` (static
    // contribution `node_size` = sizeof(ContextNode<...>)). Returns the
    // number of bytes newly added to buffered_size: `node_size` the first
    // time this identity is seen, 0 on every subsequent call while at
    // least one reference is outstanding.
    std::size_t acquire(const void *node_id, std::size_t node_size) {
        auto [it, inserted] = counts_.try_emplace(node_id, Entry{node_size, 0});
        ++it->second.refcount;
        return inserted ? node_size : 0;
    }

    // Releases one reference to `node_id`. Returns the number of bytes to
    // subtract from buffered_size: `node_size` if this was the last
    // outstanding reference, 0 if other buffered events still retain it.
    std::size_t release(const void *node_id) {
        auto it = counts_.find(node_id);
        if (it == counts_.end()) {
            return 0; // defensive: balanced acquire/release should prevent this
        }
        std::size_t freed = 0;
        if (--it->second.refcount == 0) {
            freed = it->second.size;
            counts_.erase(it);
        }
        return freed;
    }

    bool empty() const { return counts_.empty(); }

  private:
    struct Entry {
        std::size_t size;
        std::size_t refcount;
    };
    std::unordered_map<const void *, Entry> counts_;
};

// ===========================================================================
// walk_context_chain
//
// Walks a context chain from `node` up to (but excluding) RootNode,
// invoking `visit(node_identity, node_static_size)` for every node.
//
// Adapted to the real shapes in context.hpp:
//   - Context<NodeT>::underlying_node() returns shared_ptr<const NodeT>.
//   - ContextNode<T, ParentNode>::parent is shared_ptr<const ParentNode>.
// so this walks shared_ptr<const NodeT> throughout, not shared_ptr<NodeT>.
//
// The chain's static type is fully known at the log call site, so this
// recursion is resolved entirely at compile time via ordinary recursive
// template instantiation, terminating in the is_same_v<NodeT, RootNode>
// branch — no runtime type dispatch.
//
// `node.get()` is a valid identity for as long as the caller holds the
// shared_ptr (true throughout buffering: the leaf is retained by the
// buffer's ContextHolder for the entire time it is being accounted for).
// ===========================================================================
template <typename NodeT, typename Visitor>
void walk_context_chain(const std::shared_ptr<const NodeT> &node, Visitor &&visit) {
    if constexpr (std::is_same_v<NodeT, RootNode>) {
        return; // RootNode carries no payload and terminates every chain.
    } else {
        if (!node)
            return;
        visit(static_cast<const void *>(node.get()), sizeof(NodeT));
        walk_context_chain(node->parent, visit);
    }
}

template <typename NodeT, typename Visitor> void walk_context_chain(const Context<NodeT> &ctx, Visitor &&visit) {
    walk_context_chain(ctx.underlying_node(), std::forward<Visitor>(visit));
}

// ===========================================================================
// LoggerState
//
// Everything needed to eventually emit a buffered event, kept alive
// independently of the public Logger object's lifetime: a buffered event
// retains this (via a shared_ptr) so it stays emittable even if the
// Logger that produced it has already been destroyed.
//
// `targets` is moved here from what used to be Logger::targets_ — this is
// the one structural change to logger.hpp's existing data layout that
// buffering requires: the tuple of TargetT... has to live somewhere that
// can outlive Logger, and LoggerState is that place.
//
// `policy` and `capacity` are `const`, fixed at construction: there is no
// mutable-configuration API. This is deliberate — buffering is an
// immutable, construction-time property of a Logger for its entire
// lifetime, which is what removes any question of what a policy change
// from another thread would mean.
//
// IMPORTANT LIFETIME NOTE: Target<RendererT> holds a raw, non-owning
// Sink*. Previously the implicit contract was "the Sink must outlive the
// Logger" (targets_ died with Logger). Now that targets live in
// LoggerState, which can survive slightly past Logger's own destruction
// while buffered events remain pending on some thread, the contract
// becomes "the Sink must outlive LoggerState" — i.e. it must remain valid
// until every thread that ever buffered through this logger has flushed
// or terminated. This is a genuine, new lifetime requirement introduced
// by buffering, not something this file can enforce on its own.
// ===========================================================================
template <typename... TargetT> struct LoggerState {
    const LoggerId id;
    const BufferingPolicy policy;
    const std::size_t capacity;
    std::tuple<TargetT...> targets;

    // Mutable, unlike policy/capacity above: the minimum log level is
    // meant to be adjustable at runtime (e.g. raising verbosity live for
    // debugging), not fixed at construction. It lives here rather than
    // directly on Logger so that every Logger handle sharing this
    // LoggerState (via copy — Logger's copy ctor is shared_ptr-based)
    // observes the same threshold consistently. Same atomic-relaxed
    // pattern as the original log_level_filtering.hpp reference.
    std::atomic<LogLevel> minimum_level{LogLevel::trace};

    LoggerState(LoggerId id_, BufferingPolicy policy_, std::size_t capacity_, std::tuple<TargetT...> targets_)
        : id(id_), policy(policy_), capacity(capacity_), targets(std::move(targets_)) {}

    // Same logic as the old Logger::write(): apply the (event, context)
    // pair to every target. This is now the single place that does this
    // — Logger itself no longer needs its own copy of this loop.
    template <typename NodeT> void emit_now(const LogEvent &event, const Context<NodeT> &context) {
        std::apply([&](auto &...target) { (target.write(event, context), ...); }, targets);
    }
};

// ===========================================================================
// IContextHolder / ContextHolder
//
// A buffered event's context chain has a fully-static type at the log
// call site, but a single thread-local buffer must hold events coming
// from arbitrarily many different context chain types. This is the one
// place that erases that static type, via ordinary virtual dispatch — one
// small heap allocation per buffered event's context holder. If that cost
// matters for a given workload, the alternative is to template the whole
// buffering path on the context type instead and pay for one
// buffer-machinery instantiation per distinct context shape — a real
// trade-off, not something to bury silently here.
// ===========================================================================
template <typename... TargetT> struct IContextHolder {
    virtual ~IContextHolder() = default;
    virtual void emit_via(LoggerState<TargetT...> &state, const LogEvent &ev) const = 0;

    // Releases this holder's contribution from `refs`, subtracting from
    // `buffered_size` whatever ContextRefCounts::release() reports freed
    // for each node in the chain (0 for a node still retained by another
    // buffered event).
    virtual void release(ContextRefCounts &refs, std::size_t &buffered_size) const = 0;
};

template <typename NodeT, typename... TargetT> struct ContextHolder final : IContextHolder<TargetT...> {
    std::shared_ptr<const NodeT> leaf;

    explicit ContextHolder(std::shared_ptr<const NodeT> leaf_) : leaf(std::move(leaf_)) {}

    void emit_via(LoggerState<TargetT...> &state, const LogEvent &ev) const override {
        Context<NodeT> ctx(leaf);
        state.emit_now(ev, ctx);
    }

    void release(ContextRefCounts &refs, std::size_t &buffered_size) const override {
        walk_context_chain(leaf, [&](const void *id, std::size_t) { buffered_size -= refs.release(id); });
    }
};

// ===========================================================================
// BufferedEvent
//
// The owning representation buffering needs on top of the existing,
// non-owning LogEvent (event.hpp is untouched). Copy AND move are both
// deleted, deliberately: event.message is a std::string_view into
// message_storage, which lives in *this* object. If BufferedEvent were
// movable, moving it would (for short strings under the small-string-
// optimization threshold) physically relocate the message bytes, silently
// dangling any string_view captured before the move. Deleting move/copy
// makes that bug impossible to reintroduce by construction: this type can
// only live in a container that constructs elements in place and never
// relocates existing ones — exactly what std::deque guarantees for
// emplace_back, and exactly what std::vector does NOT guarantee across a
// reallocation. If the buffer's container is ever changed to a vector,
// this type simply stops compiling instead of producing a rare,
// allocation-pattern-dependent dangling pointer.
// ===========================================================================
template <typename... TargetT> struct BufferedEvent {
    std::string message_storage;
    std::unique_ptr<IContextHolder<TargetT...>> context;
    LogEvent event; // event.message views message_storage — see above

    BufferedEvent(std::string msg, LogLevel level, std::chrono::system_clock::time_point ts,
                  std::unique_ptr<IContextHolder<TargetT...>> ctx)
        : message_storage(std::move(msg)), context(std::move(ctx)),
          event{.level = level, .timestamp = ts, .message = std::string_view{message_storage}} {}

    BufferedEvent(const BufferedEvent &) = delete;
    BufferedEvent &operator=(const BufferedEvent &) = delete;
    BufferedEvent(BufferedEvent &&) = delete;
    BufferedEvent &operator=(BufferedEvent &&) = delete;
};

// ===========================================================================
// ThreadLocalBuffer
//
// Per-thread, per-logger buffering state:
//   - `strong`: strong ownership of LoggerState, held ONLY while the
//     buffer is non-empty. This is what lets a buffered event keep the
//     pipeline alive independently of the public Logger object, without
//     every TLS entry permanently pinning every logger's resources for
//     the life of the thread.
//   - `weak`: a cheap, always-held weak reference, used only to detect
//     (during lazy cleanup) that the owning Logger no longer exists.
//   - `events` / `context_refs` / `buffered_size`: the actual buffer and
//     its accounting.
//
// The destructor is the thread-termination flush path: when the
// enclosing thread_local map is destroyed at thread exit, each
// ThreadLocalBuffer's destructor runs and, if it still holds a strong
// reference (i.e. it was non-empty), attempts to flush. Any exception
// from the pipeline during this implicit flush is swallowed — a
// destructor invoked during thread teardown must not throw: remaining
// events are discarded rather than risking std::terminate. This is
// intentionally different from explicit Logger::flush(), which
// propagates exceptions normally.
// ===========================================================================
template <typename... TargetT> struct ThreadLocalBuffer {
    std::shared_ptr<LoggerState<TargetT...>> strong;
    std::weak_ptr<LoggerState<TargetT...>> weak;
    std::deque<BufferedEvent<TargetT...>> events;
    ContextRefCounts context_refs;
    std::size_t buffered_size = 0;

    bool empty() const { return events.empty(); }

    ~ThreadLocalBuffer() {
        if (events.empty() || !strong)
            return;
        try {
            while (!events.empty()) {
                auto &buffered = events.front();
                buffered.context->emit_via(*strong, buffered.event);
                events.pop_front();
            }
        } catch (...) {
            // Swallowed intentionally: remaining `events` are discarded
            // when this object finishes destructing.
        }
    }
};

} // namespace logchain
