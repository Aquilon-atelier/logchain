#pragma once

#include "context.hpp"
#include "detail/timestamp.hpp"
#include "event.hpp"

#include <cstddef>
#include <meta>
#include <string>
#include <string_view>
#include <type_traits>

#if defined(_WIN32)
#include <io.h>
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#define LOGCHAIN_ISATTY _isatty
#else
#include <unistd.h>
#define LOGCHAIN_ISATTY isatty
#endif

namespace logchain {

enum class ColorMode { Auto, Always, Never };

namespace detail {

// ===========================================================================
// Console colors
// ===========================================================================

inline constexpr std::string_view ansi_reset = "\033[0m";
inline constexpr std::string_view ansi_gray = "\033[90m";
inline constexpr std::string_view ansi_cyan = "\033[36m";
inline constexpr std::string_view ansi_green = "\033[32m";
inline constexpr std::string_view ansi_yellow = "\033[33m";
inline constexpr std::string_view ansi_red = "\033[31m";
inline constexpr std::string_view ansi_bright_red = "\033[91m";

inline constexpr std::string_view console_level_name(LogLevel level) {

    switch (level) {
    case LogLevel::trace:
        return "TRACE";

    case LogLevel::debug:
        return "DEBUG";

    case LogLevel::info:
        return "INFO ";

    case LogLevel::warning:
        return "WARN ";

    case LogLevel::error:
        return "ERROR";

    case LogLevel::fatal:
        return "FATAL";
    }

    return "?????";
}

inline constexpr std::string_view console_level_color(LogLevel level) {

    switch (level) {
    case LogLevel::trace:
        return ansi_gray;

    case LogLevel::debug:
        return ansi_cyan;

    case LogLevel::info:
        return ansi_green;

    case LogLevel::warning:
        return ansi_yellow;

    case LogLevel::error:
        return ansi_red;

    case LogLevel::fatal:
        return ansi_bright_red;
    }

    return {};
}

inline bool console_supports_color() {

#if defined(_WIN32)
    return LOGCHAIN_ISATTY(STDOUT_FILENO) != 0;
#else
    return LOGCHAIN_ISATTY(STDOUT_FILENO) != 0;
#endif
}

inline void append_console_level(std::string &out, LogLevel level, ColorMode colors) {

    const auto name = console_level_name(level);

    bool use_color = false;

    switch (colors) {
    case ColorMode::Always:
        use_color = true;
        break;

    case ColorMode::Never:
        use_color = false;
        break;

    case ColorMode::Auto:
        use_color = console_supports_color();
        break;
    }

    if (use_color) {
        out += console_level_color(level);
    }

    out += name;

    if (use_color) {
        out += ansi_reset;
    }
}

// ===========================================================================
// Console string rendering
// ===========================================================================

inline void append_console_string(std::string &out, std::string_view value) {

    bool needs_quotes = false;

    for (const char ch : value) {
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '"' || ch == '\\') {

            needs_quotes = true;
            break;
        }
    }

    if (!needs_quotes) {
        out += value;
        return;
    }

    out.push_back('"');

    for (const char ch : value) {
        switch (ch) {
        case '"':
            out += "\\\"";
            break;

        case '\\':
            out += "\\\\";
            break;

        case '\n':
            out += "\\n";
            break;

        case '\r':
            out += "\\r";
            break;

        case '\t':
            out += "\\t";
            break;

        default:
            out.push_back(ch);
            break;
        }
    }

    out.push_back('"');
}

// ===========================================================================
// Console aggregate reflection
// ===========================================================================

template <typename T> struct ConsoleMembersOf {
    static constexpr auto value =
        std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()));
};

template <typename T> void append_console_value(std::string &out, const T &value);

template <typename T, std::size_t I, std::size_t N> void append_console_members(const T &obj, std::string &out) {

    if constexpr (I < N) {
        constexpr auto member = ConsoleMembersOf<T>::value[I];

        if constexpr (I > 0) {
            out += ' ';
        }

        constexpr auto name = std::define_static_string(std::meta::identifier_of(member));

        out += std::string_view(name);
        out += '=';

        append_console_value(out, obj.[:member:]);

        append_console_members<T, I + 1, N>(obj, out);
    }
}

// ===========================================================================
// Console value rendering
// ===========================================================================

template <typename T> void append_console_value(std::string &out, const T &value) {

    using D = std::decay_t<T>;

    if constexpr (std::is_same_v<D, std::string> || std::is_same_v<D, std::string_view>) {

        append_console_string(out, value);

    } else if constexpr (std::is_same_v<D, bool>) {

        out += value ? "true" : "false";

    } else if constexpr (std::is_arithmetic_v<D>) {

        out += std::to_string(value);

    } else if constexpr (std::is_class_v<D> && std::is_aggregate_v<D>) {

        constexpr std::size_t member_count = ConsoleMembersOf<D>::value.size();

        out.push_back('{');

        append_console_members<D, 0, member_count>(value, out);

        out.push_back('}');

    } else {

        static_assert(!sizeof(T), "ConsoleRenderer: type non pris en charge.");
    }
}

// ===========================================================================
// Context traversal
// ===========================================================================

inline void append_console_fields(const RootNode &, std::string &) {}

template <typename T, typename ParentNode>
void append_console_fields(const ContextNode<T, ParentNode> &node, std::string &out) {

    // Parents first: preserve source argument order.
    append_console_fields(*node.parent, out);

    out += "  ";
    out += node.name;
    out += '=';

    append_console_value(out, node.value);
}

} // namespace detail

// ===========================================================================
// Console renderer
// ===========================================================================

struct ConsoleRenderer {

    ColorMode colors = ColorMode::Auto;

    template <typename Event, typename NodeT>
    std::string render(const Event &event, const Context<NodeT> &context) const {

        std::string out;

        out.reserve(event.message.size() + 64);

        out += detail::format_timestamp(event.timestamp);
        out += " [";

        detail::append_console_level(out, event.level, colors);

        out += "] ";
        out += event.message;

        detail::append_console_fields(*context.underlying_node(), out);

        return out;
    }
};

} // namespace logchain

#undef LOGCHAIN_ISATTY
