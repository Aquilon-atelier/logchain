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

// Journal mode of ConsoleRenderer (JournalMode).
//
// The "<N>" priority prefix (sd-daemon convention) is checked with
// JournalMode::Always, so these tests need neither systemd nor journald.
//
// The detection of the journal (JOURNAL_STREAM) is tested through
// detail::journal_stdout_connected(), which is not cached. JournalMode::Auto
// caches that result for the whole process, so it is never exercised with a
// modified environment here.
//
// Expected line numbers are taken with __LINE__ on the SAME source line as the
// logging call under test: do not reformat these lines without adapting the test.

#include <gtest/gtest.h>

#include <logchain/logger.hpp>

#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

using namespace logchain;

struct CaptureSink final : Sink {
    std::vector<std::string> lines;

    void write(std::string_view message) override { lines.emplace_back(message); }

    [[nodiscard]] const std::string &last() const { return lines.back(); }
};

bool contains(const std::string &s, std::string_view needle) { return s.find(needle) != std::string::npos; }

const std::string &this_file_basename() {
    static const std::string base{detail::source_basename(__FILE__)};
    return base;
}

// "<file>:<line>] " : the end of the located console prefix.
std::string where(int line) { return this_file_basename() + ":" + std::to_string(line) + "] "; }

// ===========================================================================
// Rendering with JournalMode::Always / Never
// ===========================================================================

TEST(JournalMode, DefaultIsAuto) {
    static_assert(ConsoleRenderer{}.journal == JournalMode::Auto);

    SUCCEED();
}

// `journal` is the last member: designated initializers written before it
// existed (colors, source, stacktrace, in that order) must keep compiling.
TEST(JournalMode, ExistingDesignatedInitializersStillCompile) {
    const ConsoleRenderer renderer{.colors = ColorMode::Never, .source = SourceMode::Short, .stacktrace = false};

    EXPECT_EQ(renderer.journal, JournalMode::Auto);
}

TEST(JournalMode, AlwaysPrefixesEachLevelWithItsPriority) {
    struct LevelPriority {
        LogLevel level;
        std::string_view prefix;
    };

    constexpr LevelPriority cases[] = {
        {LogLevel::trace, "<7>"}, {LogLevel::debug, "<7>"}, {LogLevel::info, "<6>"},
        {LogLevel::warning, "<4>"}, {LogLevel::error, "<3>"}, {LogLevel::fatal, "<2>"},
    };

    for (const auto &c : cases) {
        CaptureSink sink;
        Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never, .journal = JournalMode::Always}, sink}};

        lg.log(c.level, "m");

        EXPECT_TRUE(sink.last().starts_with(std::string(c.prefix) + "[")) << to_string(c.level) << ": " << sink.last();
    }
}

// journald records its own timestamp: the line starts with the level.
TEST(JournalMode, AlwaysOmitsTheTimestamp) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never, .journal = JournalMode::Always}, sink}};

    lg.log(LogLevel::info, "hello");

    EXPECT_EQ(sink.last(), "<6>[INFO ] hello");
}

TEST(JournalMode, AlwaysKeepsTheContextFields) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never, .journal = JournalMode::Always}, sink}};

    lg.log(LogLevel::warning, "fields {}", 42, named("user", std::string("bob")), named("id", 7));

    EXPECT_EQ(sink.last(), "<4>[WARN ] fields 42  user=bob  id=7");
}

TEST(JournalMode, NeverLeavesTheOutputUnchanged) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never, .journal = JournalMode::Never}, sink}};

    lg.log(LogLevel::info, "hello");

    const auto &s = sink.last();
    EXPECT_TRUE(contains(s, " [INFO ] hello"));
    EXPECT_NE(s.front(), '<');
    EXPECT_NE(s, "[INFO ] hello");
}

// Located layout: [LEVEL:file:line] without the timestamp.
TEST(JournalMode, AlwaysWithSourceShortDropsTheTimestampFromThePrefix) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never, .source = SourceMode::Short,
                                     .journal = JournalMode::Always},
                     sink}};

    int line = 0;
    lg.log(LogLevel::info, "located", named("k", 1)); line = __LINE__;

    EXPECT_EQ(sink.last(), "<6>[INFO:" + where(line) + "located  k=1");
}

TEST(JournalMode, AlwaysWithSourceFullKeepsTheFunctionField) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never, .source = SourceMode::Full,
                                     .journal = JournalMode::Always},
                     sink}};

    int line = 0;
    lg.log(LogLevel::error, "full"); line = __LINE__;

    const auto &s = sink.last();
    EXPECT_TRUE(s.starts_with("<3>[ERROR:"));
    EXPECT_TRUE(contains(s, ":" + std::to_string(line) + "] full  function="));
}

// journald turns each line into its own entry: every line of a multi-line
// message must carry the priority.
TEST(JournalMode, AlwaysPrefixesEachLineOfAMultiLineMessage) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never, .journal = JournalMode::Always}, sink}};

    lg.log(LogLevel::error, "first\nsecond\nthird");

    EXPECT_EQ(sink.last(), "<3>[ERROR] first\n<3>second\n<3>third");
}

// Colors are independent of the journal mode (ColorMode::Always is honored).
TEST(JournalMode, AlwaysDoesNotDisableExplicitColors) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Always, .journal = JournalMode::Always}, sink}};

    lg.log(LogLevel::info, "color");

    const auto &s = sink.last();
    EXPECT_TRUE(s.starts_with("<6>"));
    EXPECT_TRUE(contains(s, "\033[32mINFO \033[0m"));
}

// ===========================================================================
// Helpers of the journal mode
// ===========================================================================

TEST(JournalMode, PriorityMapping) {
    static_assert(detail::journal_priority(LogLevel::trace) == 7);
    static_assert(detail::journal_priority(LogLevel::debug) == 7);
    static_assert(detail::journal_priority(LogLevel::info) == 6);
    static_assert(detail::journal_priority(LogLevel::warning) == 4);
    static_assert(detail::journal_priority(LogLevel::error) == 3);
    static_assert(detail::journal_priority(LogLevel::fatal) == 2);

    SUCCEED();
}

// The shape of a rendered error event with a stack trace.
TEST(JournalMode, ApplyJournalPriorityPrefixesEveryLine) {
    std::string text = "[ERROR] boom\n  #0 app::run() at /src/app.cpp:42\n  #1 main at /src/main.cpp:10";

    detail::apply_journal_priority(text, LogLevel::error);

    EXPECT_EQ(text, "<3>[ERROR] boom\n<3>  #0 app::run() at /src/app.cpp:42\n<3>  #1 main at /src/main.cpp:10");
}

TEST(JournalMode, ApplyJournalPriorityOnASingleLine) {
    std::string text = "one line";

    detail::apply_journal_priority(text, LogLevel::info);

    EXPECT_EQ(text, "<6>one line");
}

TEST(JournalMode, ApplyJournalPriorityOnEmptyText) {
    std::string text;

    detail::apply_journal_priority(text, LogLevel::info);

    EXPECT_EQ(text, "<6>");
}

// ===========================================================================
// Detection of the journal (POSIX)
// ===========================================================================

#if !defined(_WIN32)

// Restores JOURNAL_STREAM when the test ends, whatever the test did with it.
class ScopedJournalStream {
  public:
    ScopedJournalStream() {
        if (const char *old = std::getenv("JOURNAL_STREAM")) {
            saved_ = old;
        }
    }

    ~ScopedJournalStream() {
        if (saved_) {
            setenv("JOURNAL_STREAM", saved_->c_str(), 1);
        } else {
            unsetenv("JOURNAL_STREAM");
        }
    }

    ScopedJournalStream(const ScopedJournalStream &) = delete;
    ScopedJournalStream &operator=(const ScopedJournalStream &) = delete;

    static void set(const std::string &value) { setenv("JOURNAL_STREAM", value.c_str(), 1); }
    static void unset() { unsetenv("JOURNAL_STREAM"); }

  private:
    std::optional<std::string> saved_;
};

// "<device>:<inode>" of this process's stdout, shifted to build values that do
// NOT match it.
std::string stdout_stream_id(unsigned long long device_shift = 0, unsigned long long inode_shift = 0) {
    struct stat st;

    EXPECT_EQ(fstat(STDOUT_FILENO, &st), 0);

    return std::to_string(static_cast<unsigned long long>(st.st_dev) + device_shift) + ":" +
           std::to_string(static_cast<unsigned long long>(st.st_ino) + inode_shift);
}

TEST(JournalMode, DetectionIsFalseWithoutJournalStream) {
    ScopedJournalStream guard;
    ScopedJournalStream::unset();

    EXPECT_FALSE(detail::journal_stdout_connected());
}

TEST(JournalMode, DetectionIsTrueWhenJournalStreamMatchesStdout) {
    ScopedJournalStream guard;
    ScopedJournalStream::set(stdout_stream_id());

    EXPECT_TRUE(detail::journal_stdout_connected());
}

TEST(JournalMode, DetectionIsFalseWhenTheInodeDiffers) {
    ScopedJournalStream guard;
    ScopedJournalStream::set(stdout_stream_id(0, 1));

    EXPECT_FALSE(detail::journal_stdout_connected());
}

TEST(JournalMode, DetectionIsFalseWhenTheDeviceDiffers) {
    ScopedJournalStream guard;
    ScopedJournalStream::set(stdout_stream_id(1, 0));

    EXPECT_FALSE(detail::journal_stdout_connected());
}

TEST(JournalMode, DetectionRejectsMalformedValues) {
    ScopedJournalStream guard;

    const std::string valid = stdout_stream_id();
    const auto colon = valid.find(':');
    const std::string device = valid.substr(0, colon);
    const std::string inode = valid.substr(colon + 1);

    const std::vector<std::string> malformed = {
        "",
        "garbage",
        device,                 // no colon
        device + ":",           // no inode
        ":" + inode,            // no device
        device + ":" + inode + "x", // trailing garbage
        device + "x:" + inode,  // garbage in the device
        "a:b",
        device + ":" + inode + ":" + inode, // too many fields
        "-1:-1",
    };

    for (const auto &value : malformed) {
        ScopedJournalStream::set(value);

        EXPECT_FALSE(detail::journal_stdout_connected()) << "JOURNAL_STREAM=\"" << value << "\"";
    }
}

// Auto resolves to the detection result (cached for the process, but the
// environment is restored by the guards above, so both agree).
TEST(JournalMode, AutoFollowsTheDetection) {
    EXPECT_EQ(detail::console_use_journal(JournalMode::Auto), detail::journal_stdout_connected());
}

#else

TEST(JournalMode, DetectionIsAlwaysFalseOnWindows) { EXPECT_FALSE(detail::journal_stdout_connected()); }

#endif

TEST(JournalMode, AlwaysAndNeverIgnoreTheEnvironment) {
    EXPECT_TRUE(detail::console_use_journal(JournalMode::Always));
    EXPECT_FALSE(detail::console_use_journal(JournalMode::Never));
}

} // namespace
