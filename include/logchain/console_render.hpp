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
#include <logchain/stacktrace.hpp>

#include <cstddef>
#include <meta>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

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

// Resolves ColorMode once per rendered line (Auto queries the terminal).
inline bool console_use_color(ColorMode colors) {

    switch (colors) {
    case ColorMode::Always:
        return true;

    case ColorMode::Never:
        return false;

    case ColorMode::Auto:
        return console_supports_color();
    }

    return false;
}

inline void append_console_level(std::string &out, LogLevel level, bool use_color) {

    const auto name = console_level_name(level);

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
        std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()));
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

        static_assert(!sizeof(T), "ConsoleRenderer: unsupported type.");
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

// ===========================================================================
// Call site (SourceMode::Short / Full)
//
//   [<timestamp>:<LEVEL>:<file>:<line>] <message>  <fields>
//
// Full additionally ends the line with a  function="..."  field.
// Without color the text is identical; with color, everything but the level
// is gray. Events without a location keep the default layout.
// ===========================================================================

inline void append_console_gray(std::string &out, std::string_view text, bool use_color) {

    if (use_color) {
        out += ansi_gray;
    }

    out += text;

    if (use_color) {
        out += ansi_reset;
    }
}

template <typename Event>
void append_console_located_prefix(std::string &out, const Event &event, SourceMode mode, bool use_color) {

    std::string_view level_name = console_level_name(event.level);

    while (!level_name.empty() && level_name.back() == ' ') {
        level_name.remove_suffix(1);
    }

    const std::string_view file = event.where->file_name();

    std::string head = "[";
    head += format_timestamp(event.timestamp);
    head += ':';

    std::string tail = ":";
    tail += (mode == SourceMode::Short) ? source_basename(file) : file;
    tail += ':';
    tail += std::to_string(event.where->line());
    tail += ']';

    append_console_gray(out, head, use_color);

    if (use_color) {
        out += console_level_color(event.level);
    }

    out += level_name;

    if (use_color) {
        out += ansi_reset;
    }

    append_console_gray(out, tail, use_color);

    out += ' ';
}

template <typename Event> void append_console_function(std::string &out, const Event &event, bool use_color) {

    std::string field = "function=";

    append_console_string(field, event.where->function_name());

    // The two spaces separating fields stay outside the gray run.
    out += "  ";
    append_console_gray(out, field, use_color);
}

// ===========================================================================
// Stack trace (see stacktrace.hpp for when one is printed)
//
//   <the usual event line>
//     #0 app::Service::run() at /src/service.cpp:42
//     #1 main at /src/main.cpp:10
//
// One line per frame, below the event line, in gray when colors are enabled.
// The file part is omitted when the file is unknown; so is the line when it
// is zero (glibc frames, for instance, have no debug information).
// ===========================================================================

inline void append_console_stacktrace(std::string &out, const std::vector<StackFrame> &frames, bool use_color) {

    for (std::size_t i = 0; i < frames.size(); ++i) {
        const StackFrame &frame = frames[i];

        std::string text = "  #";
        text += std::to_string(i);
        text += ' ';
        text += frame.function.empty() ? std::string_view{"??"} : std::string_view{frame.function};

        if (!frame.file.empty()) {
            text += " at ";
            text += frame.file;

            if (frame.line != 0) {
                text += ':';
                text += std::to_string(frame.line);
            }
        }

        out += '\n';
        append_console_gray(out, text, use_color);
    }
}

} // namespace detail

// ===========================================================================
// Console renderer
// ===========================================================================

struct ConsoleRenderer {

    ColorMode colors = ColorMode::Auto;
    SourceMode source = SourceMode::Never;
    // Print the stack trace of a traced exception passed with caused_by(...)
    // (error and fatal events only). Independent of `source`; ignored when
    // LOGCHAIN_WITH_STACKTRACE is not defined.
    bool stacktrace = true;

    template <typename Event, typename NodeT>
    std::string render(const Event &event, const Context<NodeT> &context) const {

        std::string out;

        out.reserve(event.message.size() + 96);

        const bool use_color = detail::console_use_color(colors);
        const bool located = source != SourceMode::Never && event.where.has_value();

        if (located) {
            detail::append_console_located_prefix(out, event, source, use_color);
        } else {
            out += detail::format_timestamp(event.timestamp);
            out += " [";

            detail::append_console_level(out, event.level, use_color);

            out += "] ";
        }

        out += event.message;

        detail::append_console_fields(*context.underlying_node(), out);

        if (located && source == SourceMode::Full) {
            detail::append_console_function(out, event, use_color);
        }

        if (stacktrace) {
            detail::append_console_stacktrace(out, detail::resolve_event_stack_frames(event), use_color);
        }

        return out;
    }
};

} // namespace logchain

#undef LOGCHAIN_ISATTY
