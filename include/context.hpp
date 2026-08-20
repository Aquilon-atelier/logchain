#pragma once

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace logchain {

// ===========================================================================
// Persistent context: immutable node + root sentinel
//
// Each node now carries a NAME (const char*, a static-lifetime literal
// provided by the LOG macro via source text stringification; see
// log_macro.hpp) in addition to its value. The name is neither inferred
// nor reflected here: context.hpp remains agnostic regarding how the
// name was obtained.
// ===========================================================================

struct RootNode {};

template <typename T, typename ParentNode> struct ContextNode {
    const char *name;
    T value;
    std::shared_ptr<const ParentNode> parent;
};

template <typename NodeT> class Context {
  public:
    using node_type = NodeT;

    explicit Context(std::shared_ptr<const NodeT> node) : node_(std::move(node)) {}

    template <typename U> auto bind(const char *name, U &&value) const {
        using StoredT = std::decay_t<U>;
        // Parenthesized aggregate initialization (P0960, C++20): a single
        // allocation for the new node, no intermediate copies.
        auto new_node = std::make_shared<const ContextNode<StoredT, NodeT>>(name, std::forward<U>(value), node_);
        return Context<ContextNode<StoredT, NodeT>>(std::move(new_node));
    }

    const std::shared_ptr<const NodeT> &underlying_node() const { return node_; }

  private:
    std::shared_ptr<const NodeT> node_; // recopié (refcount++), jamais réalloué
};

inline Context<RootNode> root_context() { return Context<RootNode>(std::make_shared<const RootNode>()); }

// ===========================================================================
// Construction from a (name, value) pack: template recursion
// (an assignment fold expression does not type-check here; see spec §4)
//
// Each argument arrives as a Named<T> (name + value), produced by the
// NAMED_ARG / LOG macro — see log_macro.hpp. make_context() completely
// ignores HOW the name was obtained; it simply consumes Named<T> objects.
// ===========================================================================

namespace detail {

template <typename T> struct Named {
    const char *name;
    T value;
};

template <typename T> Named<std::decay_t<T>> make_named(const char *name, T &&value) {
    return Named<std::decay_t<T>>{name, std::forward<T>(value)};
}

} // namespace detail

template <typename NodeT> auto make_context_impl(Context<NodeT> ctx) { return ctx; }

template <typename NodeT, typename FirstT, typename... RestT>
auto make_context_impl(Context<NodeT> ctx, detail::Named<FirstT> first, detail::Named<RestT>... rest) {
    return make_context_impl(ctx.bind(first.name, std::move(first.value)), std::move(rest)...);
}

template <typename... Ts> auto make_context(detail::Named<Ts>... named_args) {
    return make_context_impl(root_context(), std::move(named_args)...);
}

} // namespace logchain
