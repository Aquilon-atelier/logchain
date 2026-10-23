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

// Call-site capture (std::source_location) and SourceMode rendering.
//
// Expected line numbers are taken with __LINE__ on the SAME source line as the
// logging call under test: do not reformat these lines without adapting the test.

#include <gtest/gtest.h>

#include <logchain/logger.hpp>

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

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

TEST(SourceLocation, ConsoleNeverLeavesOutputUnchanged) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never}, sink}};

    lg.log(LogLevel::error, "plain");

    EXPECT_TRUE(contains(sink.last(), " [ERROR] plain"));
    EXPECT_FALSE(contains(sink.last(), this_file_basename()));
    EXPECT_NE(sink.last().front(), '[');
}

TEST(SourceLocation, ConsoleShortPutsLocationInThePrefix) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never, .source = SourceMode::Short}, sink}};

    int line = 0;
    lg.log(LogLevel::info, "hello {}", 42, named("user", std::string("bob")), named("id", 7)); line = __LINE__;

    const auto &s = sink.last();
    EXPECT_EQ(s.front(), '[');
    EXPECT_TRUE(contains(s, ":INFO:" + where(line) + "hello 42  user=bob  id=7"));
    EXPECT_TRUE(s.ends_with("user=bob  id=7"));
    EXPECT_FALSE(contains(s, "function="));
}

TEST(SourceLocation, LevelNamesAreNotPaddedInTheLocatedPrefix) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never, .source = SourceMode::Short}, sink}};

    int line = 0;
    lg.log(LogLevel::warning, "w"); line = __LINE__;

    EXPECT_TRUE(contains(sink.last(), ":WARN:" + where(line) + "w"));
}

TEST(SourceLocation, MacroCapturesTheCallingLine) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never, .source = SourceMode::Short}, sink}};
    const std::string user = "bob";
    const int id = 7;

    int line = 0;
    LOG(lg, LogLevel::warning, "macro {}", id, extra(user)); line = __LINE__;

    EXPECT_TRUE(contains(sink.last(), ":WARN:" + where(line) + "macro 7  user=bob"));
}

TEST(SourceLocation, LevelMacroCapturesTheCallingLine) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never, .source = SourceMode::Short}, sink}};

    int line = 0;
    LOG_ERROR(lg, "level macro"); line = __LINE__;

    EXPECT_TRUE(contains(sink.last(), ":ERROR:" + where(line) + "level macro"));
}

TEST(SourceLocation, ProxyCapturesTheCallingLine) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never, .source = SourceMode::Short}, sink}};

    int line = 0;
    if (auto log = lg.info()) { log("via proxy {}", 1); line = __LINE__; }

    ASSERT_FALSE(sink.lines.empty());
    EXPECT_TRUE(contains(sink.last(), ":INFO:" + where(line) + "via proxy 1"));
}

TEST(SourceLocation, ConsoleFullAddsFunctionFieldAtTheEnd) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never, .source = SourceMode::Full}, sink}};

    int line = 0;
    lg.log(LogLevel::debug, "full", named("k", 1)); line = __LINE__;

    const auto &s = sink.last();
    EXPECT_TRUE(contains(s, ":DEBUG:"));
    EXPECT_TRUE(contains(s, ":" + std::to_string(line) + "] full  k=1  function="));
    EXPECT_TRUE(contains(s, "TestBody"));
    EXPECT_LT(s.find("k=1"), s.find("function="));
}

TEST(SourceLocation, ConsoleAlwaysKeepsLevelColorAndGraysTheRest) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Always, .source = SourceMode::Short}, sink}};

    int line = 0;
    lg.log(LogLevel::info, "color"); line = __LINE__;

    const auto &s = sink.last();
    EXPECT_TRUE(s.starts_with("\033[90m["));
    EXPECT_TRUE(contains(s, "\033[32mINFO\033[0m"));
    EXPECT_TRUE(contains(s, "\033[90m:" + where(line).substr(0, where(line).size() - 2) + "]\033[0m color"));
}

TEST(SourceLocation, JsonNeverHasNoSourceKey) {
    CaptureSink sink;
    Logger lg{Target{JsonRenderer{}, sink}};

    lg.log(LogLevel::error, "json never", named("user", std::string("bob")));

    EXPECT_FALSE(contains(sink.last(), "\"source\""));
}

TEST(SourceLocation, JsonShortPlacesSourceBetweenMessageAndContext) {
    CaptureSink sink;
    Logger lg{Target{JsonRenderer{.source = SourceMode::Short}, sink}};

    int line = 0;
    lg.log(LogLevel::error, "json short", named("user", std::string("bob"))); line = __LINE__;

    const auto &s = sink.last();
    const std::string expected =
        "\"source\":{\"file\":\"" + this_file_basename() + "\",\"line\":" + std::to_string(line) + "}";

    EXPECT_TRUE(contains(s, expected));
    EXPECT_FALSE(contains(s, "\"function\""));
    EXPECT_LT(s.find("\"message\""), s.find("\"source\""));
    EXPECT_LT(s.find("\"source\""), s.find("\"user\""));
    EXPECT_EQ(s.back(), '}');
}

TEST(SourceLocation, JsonFullAddsFunction) {
    CaptureSink sink;
    Logger lg{Target{JsonRenderer{.source = SourceMode::Full}, sink}};

    int line = 0;
    lg.log(LogLevel::error, "json full"); line = __LINE__;

    EXPECT_TRUE(contains(sink.last(), "\"line\":" + std::to_string(line)));
    EXPECT_TRUE(contains(sink.last(), "\"function\":\""));
}

TEST(SourceLocation, EventWithoutLocationKeepsTheDefaultLayoutEvenInFull) {
    const LogEvent ev{.level = LogLevel::info, .timestamp = std::chrono::system_clock::now(), .message = "m"};

    EXPECT_FALSE(ev.where.has_value());

    const auto text =
        ConsoleRenderer{.colors = ColorMode::Never, .source = SourceMode::Full}.render(ev, make_context());
    EXPECT_TRUE(contains(text, " [INFO ] m"));
    EXPECT_FALSE(contains(text, "function="));

    const auto json = JsonRenderer{.source = SourceMode::Full}.render(ev, make_context());
    EXPECT_FALSE(contains(json, "\"source\""));
}

TEST(SourceLocation, AcceptsVariousMessageTypes) {
    CaptureSink sink;
    Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never, .source = SourceMode::Short}, sink}};

    const std::string s1 = "from std::string";
    const std::string_view s2 = "from string_view";
    const char *s3 = "from const char*";
    int line = 0;

    lg.log(LogLevel::info, s1); line = __LINE__;
    EXPECT_TRUE(contains(sink.last(), where(line) + s1));

    lg.log(LogLevel::info, s2); line = __LINE__;
    EXPECT_TRUE(contains(sink.last(), where(line) + std::string(s2)));

    lg.log(LogLevel::info, s3); line = __LINE__;
    EXPECT_TRUE(contains(sink.last(), where(line) + s3));
}

} // namespace
