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

#include <logchain/context.hpp>
#include <logchain/detail/timestamp.hpp>
#include <logchain/event.hpp>

#include <cstddef>
#include <cstdio>
#include <meta>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// ===========================================================================
// JSON rendering of the context: ContextNode<T, Parent> (static, named) ->
// std::string.
//
// This file remains entirely upstream of the polymorphic boundary
// defined in sink.hpp: it produces a complete std::string, which Logger
// then passes to Sink::write(std::string_view). Sink never sees a
// name, a typed value, or anything else from the context—just
// pre-formatted text. This boundary has not changed since the previous version.
//
// Two distinct sources of "names," not to be confused:
//   - the top-level name (key such as "user", "request_id"...) comes from the
//     call site's source text, captured via macro (log_macro.hpp)—NOT from
//     reflection; this information does not exist anywhere in the type system.
//   - the name of nested aggregate members ("name" in {"name":"bob"})
//     comes from reflection on the aggregate's TYPE (nonstatic_data_members_of)
//     —this information actually exists within the type, unlike the variable
//     name at the call site.
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

// Call-site object, appended right after "message":
//   ,"source":{"file":...,"line":N}                       (Short: file basename)
//   ,"source":{"file":...,"line":N,"function":...}        (Full)
// A single reserved key keeps user context names (e.g. "file", "line") from
// colliding with the call-site fields.
inline void append_json_source(std::string &out, const std::source_location &loc, SourceMode mode) {
    const std::string_view file = loc.file_name();

    out += ",\"source\":{\"file\":";
    out += json_escape_string(mode == SourceMode::Short ? source_basename(file) : file);
    out += ",\"line\":";
    out += std::to_string(loc.line());

    if (mode == SourceMode::Full) {
        out += ",\"function\":";
        out += json_escape_string(loc.function_name());
    }

    out += '}';
}

} // namespace detail

// --- Forward declarations: to_json and the aggregate fallback call each other
//     (an aggregate member can itself be an aggregate). ----

template <typename T> std::string to_json(const T &v);

namespace detail {
template <typename T> std::string aggregate_to_json(const T &obj);
} // namespace detail

// --- Serialization of a value ----------------------------------------------
//
// Static dispatch (if constexpr). Type not covered by scalar branches
// -> attempt via reflection if it is an aggregate in the language sense
// (is_aggregate_v) -> otherwise, explicit compilation failure.
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
        static_assert(!sizeof(T), "to_json: unsupported type. Neither a known scalar nor a "
                                  "reflectable aggregate (is_aggregate_v<T> must be true). "
                                  "Add an explicit if constexpr branch if this type is to become loggable.");
    }
}

namespace detail {

// ---------------------------------------------------------------------
// Aggregate fallback: nonstatic_data_members_of() (P2996).
//
// The array of members is exposed via a STATIC member of a struct
// templated on T (MembersOf<T>::value), never via a function parameter.
// This fixes the bug reported by your build: a function parameter is
// never a constant expression within the function body, even if the
// argument passed at the call site is static constexpr—so neither
// members.size() nor members[I] could have been used as constant
// expressions once "members" was received as const MembersT&.
// MembersOf<T>::value, as a static constexpr member, remains a constant
// expression wherever T is visible—at all recursion levels—without ever
// passing through a parameter.
template <typename T> struct MembersOf {
    static constexpr auto value =
        std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()));
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

// --- DAG traversal in original order, as a named object ---------------------
//
// Unlike the previous version (positional array), each node already carries
// its name (captured at the call site by the LOG macro): the output directly
// produces "name":value pairs, ready to be merged into the top-level object
// alongside "message".

inline void collect_json_fields(const RootNode &, std::vector<std::string> &) {}

template <typename T, typename ParentNode>
void collect_json_fields(const ContextNode<T, ParentNode> &node, std::vector<std::string> &out) {
    collect_json_fields(*node.parent, out); // parents d'abord : ordre d'origine
    std::string entry = detail::json_escape_string(node.name);
    entry += ':';
    entry += to_json(node.value);
    out.push_back(std::move(entry));
}

// ===========================================================================
// JSON Lines renderer
//
// The renderer is entirely independent from Sink.
// It produces a complete JSON object for one log event.
// ===========================================================================

struct JsonRenderer {

    SourceMode source = SourceMode::Never;

    template <typename Event, typename NodeT>
    std::string render(const Event &event, const Context<NodeT> &context) const {

        std::vector<std::string> fields;

        collect_json_fields(*context.underlying_node(), fields);

        std::string out;

        // Rough initial estimate. The string will grow if necessary.
        out.reserve(event.message.size() + 32 + fields.size() * 16);

        out += "{\"timestamp\":";
        out += detail::json_escape_string(detail::format_timestamp(event.timestamp));

        out += ",\"level\":";
        out += detail::json_escape_string(to_string(event.level));

        out += ",\"message\":";
        out += detail::json_escape_string(event.message);

        if (source != SourceMode::Never && event.where) {
            detail::append_json_source(out, *event.where, source);
        }

        for (const auto &field : fields) {
            out += ',';
            out += field;
        }

        out += '}';

        return out;
    }
};

} // namespace logchain
