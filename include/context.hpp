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
// 1. Contexte persistant : nœud immuable + sentinelle racine
// ===========================================================================

struct RootNode {};

template <typename T, typename ParentNode> struct ContextNode {
    T value;
    std::shared_ptr<const ParentNode> parent;
};

template <typename NodeT> class Context {
  public:
    using node_type = NodeT;

    explicit Context(std::shared_ptr<const NodeT> node) : node_(std::move(node)) {}

    template <typename U> auto bind(U &&value) const {
        using StoredT = std::decay_t<U>;
        // Aggregate init par parenthèses (P0960, C++20) : une seule
        // allocation pour le nouveau nœud, aucune copie intermédiaire.
        auto new_node = std::make_shared<const ContextNode<StoredT, NodeT>>(std::forward<U>(value), node_);
        return Context<ContextNode<StoredT, NodeT>>(std::move(new_node));
    }

    const std::shared_ptr<const NodeT> &underlying_node() const { return node_; }

  private:
    std::shared_ptr<const NodeT> node_; // recopié (refcount++), jamais réalloué
};

inline Context<RootNode> root_context() { return Context<RootNode>(std::make_shared<const RootNode>()); }

// ===========================================================================
// 2. Construction depuis un pack variadique : récursion de templates
//    (un fold expression à assignation ne type-check pas ici, cf. §4 spec)
// ===========================================================================

template <typename NodeT> auto make_context_impl(Context<NodeT> ctx) { return ctx; }

template <typename NodeT, typename First, typename... Rest>
auto make_context_impl(Context<NodeT> ctx, First &&first, Rest &&...rest) {
    return make_context_impl(ctx.bind(std::forward<First>(first)), std::forward<Rest>(rest)...);
}

template <typename... Args> auto make_context(Args &&...args) {
    return make_context_impl(root_context(), std::forward<Args>(args)...);
}

} // namespace logchain
