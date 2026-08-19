#pragma once

#include "context.hpp"

#include <cstdio>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// ===========================================================================
// Rendu JSON du contexte : ContextNode<T, Parent> (statique) -> std::string.
//
// Ce fichier est ENTIÈREMENT en amont de la frontière polymorphique définie
// dans sink.hpp. Il ne connaît aucune classe virtuelle, aucun Sink : il
// prend un Context<NodeT> et produit un std::string. C'est Logger (voir
// logger.hpp) qui appelle ce rendu puis passe le résultat, déjà sous forme
// de texte, à Sink::write(std::string_view) — la frontière ne bouge pas
// d'un pouce, elle reste le seul point de dispatch runtime de tout le
// pipeline.
//
// Aucune réflexion (P2996 / -freflection) n'est utilisée ici. Chaque valeur
// bind()ée dans la démo actuelle est un type scalaire (std::string, int...),
// traité par dispatch if constexpr — pas besoin de nonstatic_data_members_of
// pour ça. Un type non reconnu déclenche une erreur de compilation explicite
// plutôt qu'un comportement silencieux incorrect (voir to_json ci-dessous).
//
// Limite assumée : comme la chaîne ne porte aucun nom de propriété (design
// délibéré du contexte tagless), le rendu produit un TABLEAU JSON ordonné
// (même ordre que les arguments de logger.log), pas un OBJET à clés
// nommées. Retrouver un nom de variable nécessiterait soit de réintroduire
// un tag par bind() (rejeté au départ), soit une réflexion sur le site
// d'appel — hors scope ici.
// ===========================================================================

namespace logchain {
namespace detail {

inline std::string json_escape_string(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');

    for (char ch : s) {
        const auto c = static_cast<unsigned char>(ch);

        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\t':
            out += "\\t";
            break;
        case '\r':
            out += "\\r";
            break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            } else {
                out.push_back(ch);
            }
        }
    }

    out.push_back('"');
    return out;
}

} // namespace detail

// --- Sérialisation d'une valeur -------------------------------------------
//
// Dispatch purement statique (if constexpr sur le type décayé). Ajouter un
// type = ajouter une branche ici. Un type non couvert échoue à la
// compilation, avec un message explicite, au lieu de silencieusement
// mal sérialiser.
template <typename T> std::string to_json(const T &v) {
    using D = std::decay_t<T>;
    if constexpr (std::is_same_v<D, std::string> || std::is_same_v<D, std::string_view>) {
        return detail::json_escape_string(v);
    } else if constexpr (std::is_same_v<D, bool>) {
        return v ? "true" : "false";
    } else if constexpr (std::is_arithmetic_v<D>) {
        return std::to_string(v);
    } else {
        static_assert(!sizeof(T), "to_json: type non pris en charge sans reflexion. Ajoute une "
                                  "branche if constexpr ici, ou branche un fallback base sur "
                                  "nonstatic_data_members_of() pour serialiser les agregats par "
                                  "leurs membres (necessite GCC 16.1 -freflection, non valide "
                                  "dans ce sandbox — a tester sur ta toolchain).");
    }
}

// --- Parcours du DAG dans l'ordre d'origine --------------------------------
//
// Le nœud le plus "profond" (parent le plus lointain) est le PREMIER
// argument logué ; on doit donc descendre jusqu'à RootNode avant de
// commencer à empiler les fragments, pour retrouver l'ordre d'appel.

inline void collect_json_fields(const RootNode &, std::vector<std::string> &) {}

template <typename T, typename ParentNode>
void collect_json_fields(const ContextNode<T, ParentNode> &node, std::vector<std::string> &out) {
    collect_json_fields(*node.parent, out); // parents d'abord
    out.push_back(to_json(node.value));     // puis la valeur courante
}

template <typename NodeT> std::string context_to_json(const Context<NodeT> &ctx) {
    std::vector<std::string> fields;
    collect_json_fields(*ctx.underlying_node(), fields);

    std::string out = "[";
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (i)
            out += ',';
        out += fields[i];
    }
    out += ']';
    return out;
}

// --- Ligne de log complète --------------------------------------------------
//
// {"message": "...", "fields": [...]}
//
// C'est CETTE string, et rien d'autre, qui traverse la frontière
// polymorphique vers Sink::write().

template <typename NodeT> std::string build_log_line(const Context<NodeT> &ctx, std::string_view message) {
    std::string out = "{\"message\":";
    out += detail::json_escape_string(message);
    out += ",\"fields\":";
    out += context_to_json(ctx);
    out += "}";
    return out;
}

} // namespace logchain
