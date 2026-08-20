#pragma once

#include "context.hpp"

#include <cstddef>
#include <cstdio>
#include <meta>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// ===========================================================================
// Rendu JSON du contexte : ContextNode<T, Parent> (statique, nommé) ->
// std::string.
//
// Ce fichier reste entièrement en amont de la frontière polymorphique
// définie dans sink.hpp : il produit un std::string complet, que Logger
// passe ensuite à Sink::write(std::string_view). Sink ne voit jamais un
// nom, une valeur typée, ou quoi que ce soit du contexte — juste du texte
// déjà formé. Cette frontière n'a pas bougé depuis la version précédente.
//
// Deux sources de "nom" bien distinctes, à ne pas confondre :
//   - le nom au sommet (clé de "user", "request_id"...) vient du texte
//     source de l'appel, capturé par macro (log_macro.hpp) — PAS de la
//     réflexion, cette info n'existe nulle part dans le type système.
//   - le nom des membres d'un agrégat imbriqué ("name" dans {"name":"bob"})
//     vient de la réflexion sur le TYPE de l'agrégat (nonstatic_data_members_of)
//     — cette info existe réellement dans le type, contrairement au nom de
//     variable au site d'appel.
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
                out.push_back(static_cast<char>(c));
            }
        }
    }
    out.push_back('"');
    return out;
}

} // namespace detail

// --- Déclarations avancées : to_json et le fallback agrégat s'appellent
//     mutuellement (un membre d'agrégat peut lui-même être un agrégat). ----

template <typename T> std::string to_json(const T &v);

namespace detail {
template <typename T> std::string aggregate_to_json(const T &obj);
} // namespace detail

// --- Sérialisation d'une valeur ---------------------------------------------
//
// Dispatch statique (if constexpr). Type non couvert par les branches
// scalaires -> tentative via réflexion s'il s'agit d'un agrégat au sens du
// langage (is_aggregate_v) -> sinon échec de compilation explicite.
template <typename T> std::string to_json(const T &v) {
    using D = std::decay_t<T>;
    if constexpr (std::is_same_v<D, std::string> || std::is_same_v<D, std::string_view>) {
        return detail::json_escape_string(v);
    } else if constexpr (std::is_same_v<D, bool>) {
        return v ? "true" : "false";
    } else if constexpr (std::is_arithmetic_v<D>) {
        return std::to_string(v);
    } else if constexpr (std::is_class_v<D> && std::is_aggregate_v<D>) {
        return detail::aggregate_to_json(v);
    } else {
        static_assert(!sizeof(T), "to_json: type non pris en charge. Ni un scalaire connu, ni un "
                                  "agregat reflechissable "
                                  "is_aggregate_v<T> doit etre vrai). Ajoute une branche if "
                                  "constexpr explicite si ce type doit devenir loggable.");
    }
}

namespace detail {

// ---------------------------------------------------------------------
// Fallback agrégat : nonstatic_data_members_of() (P2996).
//
// Le tableau de membres est exposé via un membre STATIQUE d'un struct
// templaté sur T (MembersOf<T>::value), jamais via un paramètre de
// fonction. C'est la correction du bug rapporté par ton build : un
// paramètre de fonction n'est jamais une expression constante à
// l'intérieur du corps de la fonction, même si l'argument passé au site
// d'appel est static constexpr — donc ni members.size() ni members[I]
// n'auraient pu être utilisés comme expressions constantes une fois
// "members" reçu comme const MembersT&. MembersOf<T>::value, en tant que
// membre statique constexpr, reste une expression constante partout où T
// est visible, à tous les niveaux de récursion, sans jamais transiter par
// un paramètre.
template <typename T> struct MembersOf {
    static constexpr auto value =
        std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()));
};

template <typename T, std::size_t I, std::size_t N> void append_members(const T &obj, std::string &out) {
    if constexpr (I < N) {
        constexpr auto member = MembersOf<T>::value[I];
        if constexpr (I > 0) {
            out += ',';
        }
        constexpr auto name = std::define_static_string(std::meta::identifier_of(member));
        out += json_escape_string(std::string_view(name));
        out += ':';
        out += logchain::to_json(obj.[:member:]);

        append_members<T, I + 1, N>(obj, out);
    }
}

template <typename T> std::string aggregate_to_json(const T &obj) {
    constexpr std::size_t member_count = MembersOf<T>::value.size();

    std::string out = "{";
    append_members<T, 0, member_count>(obj, out);
    out += "}";
    return out;
}
} // namespace detail

// --- Parcours du DAG dans l'ordre d'origine, en objet nommé ----------------
//
// Contrairement à la version précédente (tableau positionnel), chaque nœud
// porte déjà son nom (capturé au site d'appel par la macro LOG) : le rendu
// produit directement des paires "nom":valeur, prêtes à être fusionnées
// dans l'objet top-level aux côtés de "message".

inline void collect_json_fields(const RootNode &, std::vector<std::string> &) {}

template <typename T, typename ParentNode>
void collect_json_fields(const ContextNode<T, ParentNode> &node, std::vector<std::string> &out) {
    collect_json_fields(*node.parent, out); // parents d'abord : ordre d'origine
    std::string entry = detail::json_escape_string(node.name);
    entry += ':';
    entry += to_json(node.value);
    out.push_back(std::move(entry));
}

// --- Ligne de log complète : objet plat -------------------------------------
//
// {"message": "...", "user": {"name": "bob"}, "request_id": "req-A", ...}
//
// C'est CETTE string, et rien d'autre, qui traverse la frontière
// polymorphique vers Sink::write().

template <typename NodeT> std::string build_log_line(const Context<NodeT> &ctx, std::string_view message) {
    std::vector<std::string> fields;
    collect_json_fields(*ctx.underlying_node(), fields);

    std::string out = "{\"message\":";
    out += detail::json_escape_string(message);
    for (const auto &field : fields) {
        out += ',';
        out += field;
    }
    out += "}";
    return out;
}

} // namespace logchain
