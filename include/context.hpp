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
//
// Chaque nœud porte maintenant un NOM (const char*, littéral de durée de vie
// statique fourni par la macro LOG via stringification du texte source, cf.
// log_macro.hpp) en plus de sa valeur. Le nom n'est jamais déduit ni réfléchi
// ici : context.hpp reste agnostique de la façon dont le nom a été obtenu.
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
        // Aggregate init par parenthèses (P0960, C++20) : une seule
        // allocation pour le nouveau nœud, aucune copie intermédiaire.
        auto new_node = std::make_shared<const ContextNode<StoredT, NodeT>>(name, std::forward<U>(value), node_);
        return Context<ContextNode<StoredT, NodeT>>(std::move(new_node));
    }

    const std::shared_ptr<const NodeT> &underlying_node() const { return node_; }

  private:
    std::shared_ptr<const NodeT> node_; // recopié (refcount++), jamais réalloué
};

inline Context<RootNode> root_context() { return Context<RootNode>(std::make_shared<const RootNode>()); }

// ===========================================================================
// 2. Construction depuis un pack de (nom, valeur) : récursion de templates
//    (un fold expression à assignation ne type-check pas ici, cf. spec §4)
//
// Chaque argument arrive sous forme de Named<T> (nom + valeur), produit par
// la macro NAMED_ARG / LOG — voir log_macro.hpp. make_context() ignore
// totalement COMMENT le nom a été obtenu ; il consomme juste des Named<T>.
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
