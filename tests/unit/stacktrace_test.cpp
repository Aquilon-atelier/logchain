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

// Stack traces of explicit exceptions: traced<E>, throw_traced(), caused_by(...)
// and their rendering by the console and JSON renderers.
//
// The tests that are compiled depend on LOGCHAIN_WITH_STACKTRACE:
//   - defined     : the feature itself is checked;
//   - not defined : the API must stay usable and inert (no #ifdef in user code,
//                   nothing printed).
//
// Frames are only identified through the test function itself (TestBody), which
// is always a real frame whatever the optimization level. File and line numbers
// are not asserted: they need debug information.
//
// Expected line numbers are taken with __LINE__ on the SAME source line as the
// logging call under test: do not reformat these lines without adapting the test.

#include <gtest/gtest.h>

#include <logchain/logger.hpp>

#include <chrono>
#include <cstddef>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <vector>

namespace {

using namespace logchain;

struct CaptureSink final : Sink {
    std::vector<std::string> lines;

    void write(std::string_view message) override { lines.emplace_back(message); }

    [[nodiscard]] const std::string &last() const { return lines.back(); }
};

bool contains(const std::string &s, std::string_view needle) { return s.find(needle) != std::string::npos; }

std::vector<std::string> split_lines(const std::string &text) {
    std::vector<std::string> lines;
    std::size_t begin = 0;

    while (begin <= text.size()) {
        const auto end = text.find('\n', begin);

        if (end == std::string::npos) {
            lines.push_back(text.substr(begin));
            break;
        }

        lines.push_back(text.substr(begin, end - begin));
        begin = end + 1;
    }

    return lines;
}

const std::string &this_file_basename() {
    static const std::string base{detail::source_basename(__FILE__)};
    return base;
}

// "<file>:<line>] " : the end of the located console prefix.
std::string where(int line) { return this_file_basename() + ":" + std::to_string(line) + "] "; }

// Throws a std::runtime_error: traced when the feature is enabled, plain otherwise.
[[noreturn]] void fail(const char *what) { throw_traced(std::runtime_error{what}); }

// ===========================================================================
// Without LOGCHAIN_WITH_STACKTRACE: the API is inert
// ===========================================================================

#if !LOGCHAIN_STACKTRACE_ENABLED

static_assert(std::is_same_v<traced<std::runtime_error>, std::runtime_error>);

TEST(Stacktrace, DisabledThrowTracedIsAPlainThrow) {
    bool caught = false;

    try {
        fail("boom");
    } catch (const std::runtime_error &ex) {
        caught = true;
        EXPECT_STREQ(ex.what(), "boom");
        EXPECT_TRUE(typeid(ex) == typeid(std::runtime_error));
    }

    EXPECT_TRUE(caught);
}

TEST(Stacktrace, DisabledCausedByIsAcceptedAndPrintsNoFrames) {
    CaptureSink console;
    CaptureSink json;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never}, console}, Target{JsonRenderer{}, json}};

    try {
        fail("boom");
    } catch (const std::exception &ex) {
        LOG_ERROR(lg, "request failed {}", caused_by(ex), 42);
    }

    ASSERT_EQ(console.lines.size(), std::size_t{1});
    EXPECT_TRUE(contains(console.last(), " [ERROR] request failed 42"));
    EXPECT_EQ(split_lines(console.last()).size(), std::size_t{1});

    ASSERT_EQ(json.lines.size(), std::size_t{1});
    EXPECT_FALSE(contains(json.last(), "\"stacktrace\""));
}

#endif // !LOGCHAIN_STACKTRACE_ENABLED

// ===========================================================================
// With LOGCHAIN_WITH_STACKTRACE: the exception carries the trace
// ===========================================================================

#if LOGCHAIN_STACKTRACE_ENABLED

bool trace_mentions(const std::stacktrace &trace, std::string_view needle) {
    for (const auto &entry : trace) {
        if (entry.description().find(needle) != std::string::npos) {
            return true;
        }
    }

    return false;
}

static_assert(std::is_base_of_v<std::runtime_error, traced<std::runtime_error>>);
static_assert(std::is_base_of_v<traced_base, traced<std::runtime_error>>);

TEST(Stacktrace, TracedExceptionKeepsItsTypeAndItsMessage) {
    bool caught = false;

    try {
        fail("boom");
    } catch (const std::runtime_error &ex) {
        caught = true;
        EXPECT_STREQ(ex.what(), "boom");
    }

    EXPECT_TRUE(caught);
}

TEST(Stacktrace, TraceIsCapturedAtTheThrowSite) {
    bool caught = false;

    try {
        fail("boom");
    } catch (const std::exception &ex) {
        caught = true;

        const auto *carrier = dynamic_cast<const traced_base *>(&ex);

        ASSERT_NE(carrier, nullptr);
        EXPECT_FALSE(carrier->trace().empty());
        EXPECT_TRUE(trace_mentions(carrier->trace(), "TestBody"));
    }

    EXPECT_TRUE(caught);
}

TEST(Stacktrace, ThrowTracedDoesNotWrapATracedExceptionTwice) {
    bool caught = false;

    try {
        throw_traced(traced<std::runtime_error>{std::runtime_error{"already traced"}, std::stacktrace::current()});
    } catch (const std::exception &ex) {
        caught = true;
        EXPECT_TRUE(typeid(ex) == typeid(traced<std::runtime_error>));
    }

    EXPECT_TRUE(caught);
}

// Logs `error` from a function of its own: if the trace were captured again
// when the exception is rethrown, this function would show up in it.
void rethrow_and_log_elsewhere(Logger<Target<ConsoleRenderer>> &lg, const std::exception_ptr &error) {
    try {
        std::rethrow_exception(error);
    } catch (const std::exception &ex) {
        LOG_ERROR(lg, "rethrown", caused_by(ex));
    }
}

TEST(Stacktrace, TheTraceSurvivesCopiesOfTheException) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never}, sink}};
    std::exception_ptr saved;

    try {
        fail("boom");
    } catch (...) {
        saved = std::current_exception();
    }

    ASSERT_NE(saved, nullptr);
    rethrow_and_log_elsewhere(lg, saved);

    // The trace is the one of the original throw site (under TestBody) and
    // not the one of the place where the exception was rethrown and logged.
    EXPECT_TRUE(contains(sink.last(), "TestBody"));
    EXPECT_FALSE(contains(sink.last(), "rethrow_and_log_elsewhere"));
}

// Copies the exception from a function of its own: a copy must keep the trace
// of the original, not capture the one of the place where it was copied.
traced<std::runtime_error> copy_in_another_function(const traced<std::runtime_error> &error) {
    return traced<std::runtime_error>(error);
}

TEST(Stacktrace, CopyingATracedExceptionKeepsTheTraceOfTheOriginal) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never}, sink}};

    try {
        fail("boom");
    } catch (const traced<std::runtime_error> &original) {
        const traced<std::runtime_error> copy = copy_in_another_function(original);

        LOG_ERROR(lg, "copied", caused_by(copy));
    }

    EXPECT_TRUE(contains(sink.last(), "TestBody"));
    EXPECT_FALSE(contains(sink.last(), "copy_in_another_function"));
}

TEST(Stacktrace, AnExistingExceptionCanBeWrapped) {
    bool caught = false;

    try {
        throw traced<std::logic_error>(std::logic_error{"wrapped"}, std::stacktrace::current());
    } catch (const std::logic_error &ex) {
        caught = true;
        EXPECT_STREQ(ex.what(), "wrapped");
        EXPECT_NE(dynamic_cast<const traced_base *>(&ex), nullptr);
    }

    EXPECT_TRUE(caught);
}

// --- Console ---------------------------------------------------------------

TEST(Stacktrace, ConsoleErrorPrintsOneLinePerFrameBelowTheEventLine) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never}, sink}};

    try {
        fail("boom");
    } catch (const std::exception &ex) {
        LOG_ERROR(lg, "request failed", caused_by(ex));
    }

    ASSERT_EQ(sink.lines.size(), std::size_t{1});

    const auto lines = split_lines(sink.last());

    ASSERT_GE(lines.size(), std::size_t{2});
    EXPECT_TRUE(contains(lines[0], " [ERROR] request failed"));
    EXPECT_FALSE(contains(lines[0], "#0"));

    for (std::size_t i = 1; i < lines.size(); ++i) {
        EXPECT_TRUE(lines[i].starts_with("  #" + std::to_string(i - 1) + " ")) << lines[i];
    }

    EXPECT_TRUE(contains(sink.last(), "TestBody"));
}

TEST(Stacktrace, ConsoleFatalPrintsTheFramesToo) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never}, sink}};

    try {
        fail("boom");
    } catch (const std::exception &ex) {
        LOG_FATAL(lg, "giving up", caused_by(ex));
    }

    EXPECT_TRUE(contains(sink.last(), " [FATAL] giving up\n  #0 "));
}

TEST(Stacktrace, ConsoleFramesAreGrayWhenColorsAreEnabled) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Always}, sink}};

    try {
        fail("boom");
    } catch (const std::exception &ex) {
        LOG_ERROR(lg, "request failed", caused_by(ex));
    }

    const auto lines = split_lines(sink.last());

    ASSERT_GE(lines.size(), std::size_t{2});
    EXPECT_TRUE(lines[1].starts_with("\033[90m  #0 "));
    EXPECT_TRUE(lines[1].ends_with("\033[0m"));
}

// In unoptimized builds the capture machinery (traced_base and traced<E>
// constructors, throw_traced) is on the stack at the throw site: none of it
// must be printed, and the first frame is the function that threw (or, when
// the optimizer inlined it, the one that called it).
TEST(Stacktrace, TheCaptureMachineryIsNotPartOfTheTrace) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never}, sink}};

    try {
        fail("boom");
    } catch (const std::exception &ex) {
        LOG_ERROR(lg, "request failed", caused_by(ex));
    }

    EXPECT_FALSE(contains(sink.last(), "throw_traced"));
    EXPECT_FALSE(contains(sink.last(), "traced_base"));
    EXPECT_FALSE(contains(sink.last(), "logchain::traced"));

    const auto lines = split_lines(sink.last());

    ASSERT_GE(lines.size(), std::size_t{2});
    EXPECT_TRUE(contains(lines[1], "fail") || contains(lines[1], "TestBody")) << lines[1];
}

TEST(Stacktrace, ConsoleSourceModeAndStacktraceAreIndependent) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never, .source = SourceMode::Full}, sink}};

    int line = 0;

    try {
        fail("boom");
    } catch (const std::exception &ex) { LOG_ERROR(lg, "located", caused_by(ex)); line = __LINE__; }

    const auto lines = split_lines(sink.last());

    ASSERT_GE(lines.size(), std::size_t{2});
    // Full prints the path as given to the compiler (not the basename).
    EXPECT_TRUE(contains(lines[0], ":ERROR:" + std::string{__FILE__} + ":" + std::to_string(line) + "] located  function="));
    EXPECT_TRUE(lines[1].starts_with("  #0 "));
}

// --- JSON ------------------------------------------------------------------

TEST(Stacktrace, JsonErrorPrintsAFrameArrayBetweenSourceAndContext) {
    CaptureSink sink;
    Logger lg{Target{JsonRenderer{.source = SourceMode::Short}, sink}};
    const std::string user = "bob";

    try {
        fail("boom");
    } catch (const std::exception &ex) {
        LOG_ERROR(lg, "request failed", caused_by(ex), extra(user));
    }

    ASSERT_EQ(sink.lines.size(), std::size_t{1});

    const auto &s = sink.last();

    EXPECT_TRUE(contains(s, "\"stacktrace\":[{\"function\":\""));
    EXPECT_TRUE(contains(s, "\",\"file\":\""));
    EXPECT_TRUE(contains(s, "\",\"line\":"));
    EXPECT_TRUE(contains(s, "TestBody"));
    EXPECT_TRUE(contains(s, "}],\"user\":\"bob\"}"));

    EXPECT_LT(s.find("\"message\""), s.find("\"source\""));
    EXPECT_LT(s.find("\"source\""), s.find("\"stacktrace\""));
    EXPECT_LT(s.find("\"stacktrace\""), s.find("\"user\""));
    EXPECT_EQ(split_lines(s).size(), std::size_t{1});
}

TEST(Stacktrace, JsonWithoutSourceModeStillPrintsTheFrames) {
    CaptureSink sink;
    Logger lg{Target{JsonRenderer{}, sink}};

    try {
        fail("boom");
    } catch (const std::exception &ex) {
        LOG_ERROR(lg, "request failed", caused_by(ex));
    }

    EXPECT_FALSE(contains(sink.last(), "\"source\""));
    EXPECT_TRUE(contains(sink.last(), ",\"stacktrace\":[{\"function\":\""));
    EXPECT_TRUE(sink.last().ends_with("}]}"));
}

// --- When no trace must be printed -----------------------------------------

TEST(Stacktrace, BelowErrorLevelNoFramesArePrinted) {
    CaptureSink console;
    CaptureSink json;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never}, console}, Target{JsonRenderer{}, json}};

    try {
        fail("boom");
    } catch (const std::exception &ex) {
        LOG_WARN(lg, "recovered", caused_by(ex));
        LOG_INFO(lg, "still fine", caused_by(ex));
    }

    ASSERT_EQ(console.lines.size(), std::size_t{2});
    EXPECT_EQ(split_lines(console.lines[0]).size(), std::size_t{1});
    EXPECT_EQ(split_lines(console.lines[1]).size(), std::size_t{1});

    ASSERT_EQ(json.lines.size(), std::size_t{2});
    EXPECT_FALSE(contains(json.lines[0], "\"stacktrace\""));
    EXPECT_FALSE(contains(json.lines[1], "\"stacktrace\""));
}

TEST(Stacktrace, AnExceptionThatIsNotTracedPrintsNoFrames) {
    CaptureSink console;
    CaptureSink json;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never}, console}, Target{JsonRenderer{}, json}};

    try {
        throw std::runtime_error{"plain"};
    } catch (const std::exception &ex) {
        LOG_ERROR(lg, "request failed", caused_by(ex));
    }

    EXPECT_EQ(split_lines(console.last()).size(), std::size_t{1});
    EXPECT_FALSE(contains(json.last(), "\"stacktrace\""));
}

TEST(Stacktrace, AnExceptionThatIsNotDeclaredWithCausedByPrintsNoFrames) {
    CaptureSink console;
    CaptureSink json;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never}, console}, Target{JsonRenderer{}, json}};

    try {
        fail("boom");
    } catch (const std::exception &ex) {
        LOG_ERROR(lg, "request failed: {}", ex.what());
    }

    EXPECT_TRUE(contains(console.last(), " [ERROR] request failed: boom"));
    EXPECT_EQ(split_lines(console.last()).size(), std::size_t{1});
    EXPECT_FALSE(contains(json.last(), "\"stacktrace\""));
}

TEST(Stacktrace, TheRendererOptionTurnsTheFramesOff) {
    CaptureSink console;
    CaptureSink json;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never, .stacktrace = false}, console},
              Target{JsonRenderer{.stacktrace = false}, json}};

    try {
        fail("boom");
    } catch (const std::exception &ex) {
        LOG_ERROR(lg, "request failed", caused_by(ex));
    }

    EXPECT_EQ(split_lines(console.last()).size(), std::size_t{1});
    EXPECT_FALSE(contains(json.last(), "\"stacktrace\""));
}

TEST(Stacktrace, EachTargetDecidesForItself) {
    CaptureSink console;
    CaptureSink json;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never, .stacktrace = false}, console},
              Target{JsonRenderer{}, json}};

    try {
        fail("boom");
    } catch (const std::exception &ex) {
        LOG_ERROR(lg, "request failed", caused_by(ex));
    }

    EXPECT_EQ(split_lines(console.last()).size(), std::size_t{1});
    EXPECT_TRUE(contains(json.last(), "\"stacktrace\":["));
}

#endif // LOGCHAIN_STACKTRACE_ENABLED

// ===========================================================================
// caused_by(...) in every logging API (any configuration)
// ===========================================================================

TEST(Stacktrace, CausedByIsNotAFormatArgument) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never}, sink}};

    try {
        fail("boom");
    } catch (const std::exception &ex) {
        LOG_ERROR(lg, "code {} on {}", caused_by(ex), 42, std::string{"db"});
    }

    EXPECT_TRUE(contains(split_lines(sink.last())[0], " [ERROR] code 42 on db"));
}

TEST(Stacktrace, CausedByComposesWithExtraAndNamedArguments) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never}, sink}};
    const std::string user = "bob";

    try {
        fail("boom");
    } catch (const std::exception &ex) {
        LOG_ERROR(lg, "code {}", caused_by(ex), 7, extra(user));
        EXPECT_TRUE(contains(split_lines(sink.last())[0], " [ERROR] code 7  user=bob"));

        lg.log(LogLevel::error, "code {}", caused_by(ex), 9, named("k", 1));
        EXPECT_TRUE(contains(split_lines(sink.last())[0], " [ERROR] code 9  k=1"));
    }
}

TEST(Stacktrace, CausedByWorksThroughTheLazyProxy) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never}, sink}};

    try {
        fail("boom");
    } catch (const std::exception &ex) {
        if (auto log = lg.error()) {
            log("via proxy {}", caused_by(ex), 3);
        }
    }

    ASSERT_EQ(sink.lines.size(), std::size_t{1});
    EXPECT_TRUE(contains(split_lines(sink.last())[0], " [ERROR] via proxy 3"));

#if LOGCHAIN_STACKTRACE_ENABLED
    EXPECT_TRUE(contains(sink.last(), "\n  #0 "));
#endif
}

TEST(Stacktrace, CausedByKeepsTheCallSite) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never, .source = SourceMode::Short}, sink}};

    int line = 0;

    try {
        fail("boom");
    } catch (const std::exception &ex) { LOG_ERROR(lg, "located", caused_by(ex)); line = __LINE__; }

    EXPECT_TRUE(contains(split_lines(sink.last())[0], ":ERROR:" + where(line) + "located"));
}

TEST(Stacktrace, TheMessageIsNotEvaluatedWhenTheLevelIsDisabled) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never}, sink}};
    int calls = 0;
    auto expensive = [&calls] {
        ++calls;
        return 1;
    };

    lg.set_minimum_level(LogLevel::fatal);

    try {
        fail("boom");
    } catch (const std::exception &ex) {
        LOG_ERROR(lg, "code {}", caused_by(ex), expensive());
    }

    EXPECT_EQ(calls, 0);
    EXPECT_TRUE(sink.lines.empty());
}

TEST(Stacktrace, AnEventWithoutAnExceptionPrintsNoFrames) {
    const LogEvent ev{.level = LogLevel::error, .timestamp = std::chrono::system_clock::now(), .message = "m"};

    EXPECT_EQ(ev.error, nullptr);

    const auto text = ConsoleRenderer{.colors = ColorMode::Never}.render(ev, make_context());
    EXPECT_TRUE(contains(text, " [ERROR] m"));
    EXPECT_EQ(split_lines(text).size(), std::size_t{1});

    const auto json = JsonRenderer{}.render(ev, make_context());
    EXPECT_FALSE(contains(json, "\"stacktrace\""));
}

} // namespace
