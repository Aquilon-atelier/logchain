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

// Executable documentation: the code examples given in docs/*.md, replayed and
// checked against the exact behavior each page describes.
//
// Organization: one section per documentation page, one test per example (or
// per group of tightly related examples). Each test names the page and the
// section it comes from.
//
// Adaptations, always stated in the test itself:
//   - "/tmp/application.log" is replaced by a per-test temporary directory;
//   - ConsoleSink writes into a std::ostringstream instead of std::cout;
//   - helpers that the documentation leaves undefined (expensive(),
//     get_logger()...) are replaced by small instrumented stubs;
//   - sizes such as 1 MiB are shrunk when only the rotation logic matters.
//
// "DOC MISMATCH" marks a place where the documentation, as written, does not
// match the library. The test then checks the working equivalent, so that the
// suite documents the gap until either the page or the library is fixed.
//
// Timestamps are replaced by <TS> and ANSI colors are stripped before
// comparison, so default renderers (ColorMode::Auto) are safe to use here
// whether or not the tests run in a terminal.
//
// Expected line numbers are taken with __LINE__ on the SAME source line as the
// logging call under test: do not reformat these lines without adapting the test.

#include <gtest/gtest.h>

#include <logchain/logger.hpp>

#include <array>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using namespace logchain;
namespace fs = std::filesystem;

// ===========================================================================
// Helpers
// ===========================================================================

struct CaptureSink final : Sink {
    std::vector<std::string> lines;

    void write(std::string_view message) override { lines.emplace_back(message); }

    [[nodiscard]] const std::string &last() const { return lines.back(); }
};

// A logger writing plain console text (no colors) into a CaptureSink.
struct Captured {
    CaptureSink sink;
    Logger<Target<ConsoleRenderer>> logger;

    explicit Captured(SourceMode mode = SourceMode::Never)
        : logger{Target{ConsoleRenderer{.colors = ColorMode::Never, .source = mode}, sink}} {}
};

bool contains(const std::string &s, std::string_view needle) { return s.find(needle) != std::string::npos; }

// Strips ANSI color sequences and replaces every ISO-8601 timestamp by <TS>.
std::string normalize(std::string text) {
    static const std::regex ansi{"\x1b\\[[0-9;]*m"};
    static const std::regex timestamp{R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{6}Z)"};

    text = std::regex_replace(text, ansi, "");
    return std::regex_replace(text, timestamp, "<TS>");
}

std::vector<std::string> split_lines(const std::string &text) {
    std::vector<std::string> lines;
    std::istringstream in{text};
    std::string line;

    while (std::getline(in, line)) {
        lines.push_back(line);
    }

    return lines;
}

std::vector<std::string> read_lines(const fs::path &file) {
    std::ifstream in{file};
    EXPECT_TRUE(in.is_open()) << file;

    std::vector<std::string> lines;
    std::string line;

    while (std::getline(in, line)) {
        lines.push_back(line);
    }

    return lines;
}

fs::path archive(const fs::path &file, int index) { return fs::path{file.string() + "." + std::to_string(index)}; }

// "msg-00001": 9 characters, i.e. exactly 10 bytes once '\n' is added.
std::string message_n(int i) { return std::format("msg-{:05d}", i); }

void write_messages(FileSink &sink, int first, int last) {
    for (int i = first; i <= last; ++i) {
        sink.write(message_n(i));
    }
}

const std::string &this_file_basename() {
    static const std::string base{detail::source_basename(__FILE__)};
    return base;
}

// "<file>:<line>] " : the end of the located console prefix.
std::string where(int line) { return this_file_basename() + ":" + std::to_string(line) + "] "; }

// Each test gets its own directory: ctest may run tests of this binary in
// parallel processes.
class DocExamples : public ::testing::Test {
  protected:
    fs::path test_dir;

    void SetUp() override {
        test_dir = fs::temp_directory_path() /
                   (std::string{"logchain_doc_examples_"} + ::testing::UnitTest::GetInstance()->current_test_info()->name());

        std::error_code ec;
        fs::remove_all(test_dir, ec);
        ASSERT_FALSE(ec);

        fs::create_directories(test_dir, ec);
        ASSERT_FALSE(ec);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(test_dir, ec);
    }

    [[nodiscard]] fs::path path(std::string_view name) const { return test_dir / std::string{name}; }
};

struct User {
    std::string name{"bob"};
    unsigned int age;
};

// ===========================================================================
// getting_started.md
// ===========================================================================

// "Basic usage": a console sink and a file sink, default renderers.
// Doc output is pretty-printed JSON for readability; the file really holds one
// compact JSON object per line.
// DOC MISMATCH (order): the documented JSON lists "user" before "request_id",
// but fields are rendered in the order of the extra(...) arguments:
// request_id, user, retry_count.
TEST_F(DocExamples, GettingStartedBasicUsage) {
    std::ostringstream console_out;
    ConsoleSink console_sink{console_out};
    FileSink file_sink{path("application.log")};

    Logger logger{target(console_sink), target(file_sink)};

    User user{"bob", 35};
    std::string request_id{"req-A"};
    int retry_count{3};

    LOG_INFO(logger, "Request handled", extra(request_id, user, retry_count));

    EXPECT_EQ(normalize(console_out.str()),
              "<TS> [INFO ] Request handled  request_id=req-A  user={name=bob age=35}  retry_count=3\n");

    const auto lines = read_lines(path("application.log"));
    ASSERT_EQ(lines.size(), std::size_t{1});
    EXPECT_EQ(normalize(lines[0]),
              R"({"timestamp":"<TS>","level":"INFO","message":"Request handled",)"
              R"("request_id":"req-A","user":{"name":"bob","age":35},"retry_count":3})");
}

// "The macro is optional": the explicit, macro-free API.
// DOC MISMATCH: the page shows logger.log_named(LogLevel::info, ...), which
// does not exist. The explicit form is logger.log(level, message, named(...)...).
TEST_F(DocExamples, GettingStartedExplicitApiGivesTheSameEventAsTheMacro) {
    Captured c;

    User user{"bob", 35};
    std::string request_id{"req-A"};
    int retry_count{3};

    LOG_INFO(c.logger, "Request handled", extra(request_id, user, retry_count));

    c.logger.log(LogLevel::info, "Request handled", named("request_id", request_id), named("user", user),
                 named("retry_count", retry_count));

    ASSERT_EQ(c.sink.lines.size(), std::size_t{2});
    EXPECT_EQ(normalize(c.sink.lines[0]), normalize(c.sink.lines[1]));
    EXPECT_EQ(normalize(c.sink.lines[1]),
              "<TS> [INFO ] Request handled  request_id=req-A  user={name=bob age=35}  retry_count=3");
}

// ===========================================================================
// rendering.md
// ===========================================================================

// "Using the default renderers": ConsoleSink -> ConsoleRenderer,
// FileSink -> JsonRenderer, and the rendered output of the "Example" section.
TEST_F(DocExamples, RenderingDefaultRenderers) {
    static_assert(std::is_same_v<default_renderer_for_t<ConsoleSink>, ConsoleRenderer>);
    static_assert(std::is_same_v<default_renderer_for_t<FileSink>, JsonRenderer>);

    std::ostringstream console_out;
    ConsoleSink console_sink{console_out};
    FileSink file_sink{path("application.log")};

    Logger logger{target(console_sink), target(file_sink)};

    std::string request_id{"req-A"};
    int retry_count{3};

    LOG_INFO(logger, "Request handled", extra(request_id, retry_count));

    EXPECT_EQ(normalize(console_out.str()), "<TS> [INFO ] Request handled  request_id=req-A  retry_count=3\n");

    const auto lines = read_lines(path("application.log"));
    ASSERT_EQ(lines.size(), std::size_t{1});
    EXPECT_EQ(normalize(lines[0]), R"({"timestamp":"<TS>","level":"INFO","message":"Request handled",)"
                                   R"("request_id":"req-A","retry_count":3})");
}

// "Selecting a renderer explicitly": the renderers are deliberately swapped
// and the logging call is unchanged.
// DOC MISMATCH: the page writes target(console_sink, JsonRenderer{}), but
// target() only takes a sink (it picks the default renderer). The explicit
// form is Target{JsonRenderer{}, console_sink}.
TEST_F(DocExamples, RenderingSwappedRenderers) {
    std::ostringstream console_out;
    ConsoleSink console_sink{console_out};
    FileSink file_sink{path("application.log")};

    Logger logger{Target{JsonRenderer{}, console_sink}, Target{ConsoleRenderer{}, file_sink}};

    std::string request_id{"req-A"};
    int retry_count{3};

    LOG_INFO(logger, "Request handled", extra(request_id, retry_count));

    EXPECT_EQ(normalize(console_out.str()), "{\"timestamp\":\"<TS>\",\"level\":\"INFO\",\"message\":\"Request handled\","
                                            "\"request_id\":\"req-A\",\"retry_count\":3}\n");

    const auto lines = read_lines(path("application.log"));
    ASSERT_EQ(lines.size(), std::size_t{1});
    EXPECT_EQ(normalize(lines[0]), "<TS> [INFO ] Request handled  request_id=req-A  retry_count=3");
}

// ===========================================================================
// log_rotation.md
// ===========================================================================

// "Default settings": the documented default values.
TEST_F(DocExamples, LogRotationDefaultSettings) {
    const FileSinkSettings settings{.path = "/tmp/application.log"};

    EXPECT_TRUE(settings.rotation == Rotation::enabled);
    EXPECT_EQ(settings.max_files, std::size_t{5});
    EXPECT_EQ(settings.max_file_size, std::size_t{10} * 1024 * 1024);
    EXPECT_EQ(MiB, std::size_t{1024} * 1024);
}

// "Overview": the first example (5 files, 10 MiB), used through a Logger.
TEST_F(DocExamples, LogRotationOverviewExample) {
    const auto log_path = path("application.log");

    FileSink sink(FileSinkSettings{.path = log_path, .max_files = 5, .max_file_size = 10 * logchain::MiB});

    Logger logger(target(sink));

    LOG_INFO(logger, "hello");

    EXPECT_EQ(read_lines(log_path).size(), std::size_t{1});
}

// "Default settings": FileSink(path) and FileSink(FileSinkSettings{.path})
// are equivalent. Both rotate once the 10 MiB threshold is exceeded: ten 1 MiB
// entries fit exactly, the eleventh triggers the rotation.
void expect_default_threshold(FileSink &sink, const fs::path &file) {
    const std::string one_mebibyte_entry(MiB - 1, 'x'); // + '\n' = exactly 1 MiB

    for (int i = 0; i < 10; ++i) {
        sink.write(one_mebibyte_entry);
    }
    EXPECT_FALSE(fs::exists(archive(file, 1)));

    sink.write(one_mebibyte_entry);
    EXPECT_TRUE(fs::exists(archive(file, 1)));
}

TEST_F(DocExamples, LogRotationBothConstructorsAreEquivalent) {
    const auto file_a = path("a.log");
    const auto file_b = path("b.log");

    {
        FileSink sink(file_a); // FileSink sink("/tmp/application.log");
        expect_default_threshold(sink, file_a);
    }
    {
        FileSink sink(FileSinkSettings{.path = file_b});
        expect_default_threshold(sink, file_b);
    }
}

// "Default settings": with the defaults, max_files = 5 includes the current
// file, i.e. application.log and .1 to .4 (never .5).
// The documented size (10 MiB) is shrunk to 20 bytes; max_files stays default.
TEST_F(DocExamples, LogRotationDefaultKeepsFiveFilesIncludingTheCurrentOne) {
    const auto file = path("application.log");

    FileSink sink(FileSinkSettings{.path = file, .max_file_size = 20});

    write_messages(sink, 1, 20); // 10 bytes each: 2 per file

    EXPECT_TRUE(fs::exists(file));
    for (int i = 1; i <= 4; ++i) {
        EXPECT_TRUE(fs::exists(archive(file, i))) << "application.log." << i;
    }
    EXPECT_FALSE(fs::exists(archive(file, 5)));
}

// "Configuring rotation": max_files = 3 keeps application.log, .1 and .2.
// The documented example (1 MiB) is built as written; the behavior is checked
// with a 20-byte threshold. The oldest entries are the ones discarded.
TEST_F(DocExamples, LogRotationThreeFiles) {
    const auto file = path("application.log");

    {
        // Exactly as in the documentation (only the path differs).
        FileSink documented(FileSinkSettings{.path = path("documented.log"), .max_files = 3, .max_file_size = 1024 * 1024});
        Logger logger(target(documented));
        LOG_INFO(logger, "hello");
    }

    FileSink sink(FileSinkSettings{.path = file, .max_files = 3, .max_file_size = 20});

    write_messages(sink, 1, 8);

    EXPECT_EQ(read_lines(file), (std::vector<std::string>{message_n(7), message_n(8)}));
    EXPECT_EQ(read_lines(archive(file, 1)), (std::vector<std::string>{message_n(5), message_n(6)}));
    EXPECT_EQ(read_lines(archive(file, 2)), (std::vector<std::string>{message_n(3), message_n(4)}));
    EXPECT_FALSE(fs::exists(archive(file, 3)));
}

// "Disabling rotation": max_files and max_file_size then have no effect.
TEST_F(DocExamples, LogRotationDisabled) {
    const auto file = path("application.log");

    // The documented example, extended with values that would rotate if they applied.
    FileSink sink(FileSinkSettings{.path = file, .rotation = Rotation::disabled, .max_files = 3, .max_file_size = 20});

    write_messages(sink, 1, 10);

    EXPECT_FALSE(fs::exists(archive(file, 1)));
    EXPECT_EQ(read_lines(file).size(), std::size_t{10});
}

// "max_files = 1": rotation still happens, no archive is retained, the
// current file is truncated.
TEST_F(DocExamples, LogRotationMaxFilesOneKeepsNoArchive) {
    const auto file = path("application.log");

    FileSink sink(FileSinkSettings{.path = file, .max_files = 1, .max_file_size = 20});

    write_messages(sink, 1, 5);

    EXPECT_FALSE(fs::exists(archive(file, 1)));
    EXPECT_EQ(read_lines(file), (std::vector<std::string>{message_n(5)}));
}

// "Multiple loggers": two FileSink instances on the same path are not supported.
TEST_F(DocExamples, LogRotationTwoSinksOnTheSamePathAreRejected) {
    const FileSinkSettings settings{.path = path("application.log"), .max_files = 3, .max_file_size = 1024 * 1024};

    FileSink sink1(settings);

    EXPECT_THROW(FileSink sink2(settings), std::runtime_error);
}

// "Multiple loggers": loggers that need the same file share one FileSink.
TEST_F(DocExamples, LogRotationSharedSinkBetweenTwoLoggers) {
    const auto file = path("application.log");

    FileSink sink(FileSinkSettings{.path = file, .max_files = 3, .max_file_size = 1024 * 1024});

    Logger logger1(target(sink));
    Logger logger2(target(sink));

    LOG_INFO(logger1, "from logger 1");
    LOG_INFO(logger2, "from logger 2");

    const auto lines = read_lines(file);
    ASSERT_EQ(lines.size(), std::size_t{2});
    EXPECT_TRUE(contains(lines[0], "\"message\":\"from logger 1\""));
    EXPECT_TRUE(contains(lines[1], "\"message\":\"from logger 2\""));
}

// ===========================================================================
// log_level_filtering.md
// ===========================================================================

constexpr std::array<LogLevel, 6> all_levels{LogLevel::trace,   LogLevel::debug, LogLevel::info,
                                             LogLevel::warning, LogLevel::error, LogLevel::fatal};

// "Log levels": six ordered levels.
TEST_F(DocExamples, LogLevelFilteringLevelsAreOrdered) {
    static_assert(LogLevel::trace < LogLevel::debug);
    static_assert(LogLevel::debug < LogLevel::info);
    static_assert(LogLevel::info < LogLevel::warning);
    static_assert(LogLevel::warning < LogLevel::error);
    static_assert(LogLevel::error < LogLevel::fatal);
    EXPECT_EQ(all_levels.size(), std::size_t{6});
}

// "Log levels" and "Querying the minimum level": enabled / filtered levels,
// default minimum level, and is_enabled(level) == (level >= minimum_level()).
TEST_F(DocExamples, LogLevelFilteringMinimumLevel) {
    Logger logger;

    // The default minimum level is trace: everything is enabled.
    EXPECT_TRUE(logger.minimum_level() == LogLevel::trace);
    for (const auto level : all_levels) {
        EXPECT_TRUE(logger.is_enabled(level));
    }

    logger.set_minimum_level(LogLevel::warning);

    EXPECT_TRUE(logger.minimum_level() == LogLevel::warning);

    // WARNING, ERROR, FATAL are enabled; TRACE, DEBUG, INFO are filtered out.
    EXPECT_FALSE(logger.is_enabled(LogLevel::trace));
    EXPECT_FALSE(logger.is_enabled(LogLevel::debug));
    EXPECT_FALSE(logger.is_enabled(LogLevel::info));
    EXPECT_TRUE(logger.is_enabled(LogLevel::warning));
    EXPECT_TRUE(logger.is_enabled(LogLevel::error));
    EXPECT_TRUE(logger.is_enabled(LogLevel::fatal));

    for (const auto level : all_levels) {
        EXPECT_EQ(logger.is_enabled(level), level >= logger.minimum_level());
    }
}

// "Filtering and evaluation of log arguments", macro API:
// LOG_DEBUG(logger, expensive()) does not evaluate expensive() when DEBUG is
// disabled, LOG_WARN(logger, expensive()) does.
TEST_F(DocExamples, LogLevelFilteringMacroDoesNotEvaluateDisabledArguments) {
    Captured c;
    int calls = 0;
    auto expensive = [&calls] {
        ++calls;
        return std::string{"expensive"};
    };

    c.logger.set_minimum_level(LogLevel::warning);

    LOG_DEBUG(c.logger, expensive());
    EXPECT_EQ(calls, 0);
    EXPECT_TRUE(c.sink.lines.empty());

    LOG_WARN(c.logger, expensive());
    EXPECT_EQ(calls, 1);
    ASSERT_EQ(c.sink.lines.size(), std::size_t{1});
    EXPECT_TRUE(contains(c.sink.last(), " [WARN ] expensive"));
}

// Same page, lazy proxy API: the body of the if is not executed when DEBUG is disabled.
TEST_F(DocExamples, LogLevelFilteringProxyDoesNotEvaluateDisabledArguments) {
    Captured c;
    int calls = 0;
    auto expensive = [&calls] {
        ++calls;
        return std::string{"expensive"};
    };

    c.logger.set_minimum_level(LogLevel::warning);

    if (auto log = c.logger.debug()) {
        log(expensive());
    }
    EXPECT_EQ(calls, 0);
    EXPECT_TRUE(c.sink.lines.empty());

    c.logger.set_minimum_level(LogLevel::debug);

    if (auto log = c.logger.debug()) {
        log(expensive());
    }
    EXPECT_EQ(calls, 1);
    ASSERT_EQ(c.sink.lines.size(), std::size_t{1});
    EXPECT_TRUE(contains(c.sink.last(), " [DEBUG] expensive"));
}

// "Filtering and evaluation of log arguments" and "Generic log() API":
// logger.log(level, expensive()) filters the event but cannot prevent the
// evaluation of its argument.
TEST_F(DocExamples, LogLevelFilteringGenericApiStillEvaluatesItsArguments) {
    Captured c;
    int calls = 0;
    auto expensive = [&calls] {
        ++calls;
        return std::string{"expensive"};
    };

    c.logger.set_minimum_level(LogLevel::warning);

    c.logger.log(LogLevel::debug, expensive());

    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(c.sink.lines.empty());

    c.logger.log(LogLevel::warning, "warning message");

    ASSERT_EQ(c.sink.lines.size(), std::size_t{1});
    EXPECT_TRUE(contains(c.sink.last(), " [WARN ] warning message"));
}

// "Early filtering": LOG_DEBUG(logger, "state = {}", build_expensive_debug_state())
// does not call the builder when DEBUG is disabled, and formats it when enabled.
TEST_F(DocExamples, LogLevelFilteringEarlyFilteringWithFormatArguments) {
    Captured c;
    int calls = 0;
    auto build_expensive_debug_state = [&calls] {
        ++calls;
        return std::string{"S"};
    };

    c.logger.set_minimum_level(LogLevel::warning);

    LOG_DEBUG(c.logger, "state = {}", build_expensive_debug_state());
    EXPECT_EQ(calls, 0);
    EXPECT_TRUE(c.sink.lines.empty());

    c.logger.set_minimum_level(LogLevel::debug);

    LOG_DEBUG(c.logger, "state = {}", build_expensive_debug_state());
    EXPECT_EQ(calls, 1);
    ASSERT_EQ(c.sink.lines.size(), std::size_t{1});
    EXPECT_TRUE(contains(c.sink.last(), " [DEBUG] state = S"));
}

// "Lazy level-specific API": one proxy per level, usable as an if condition,
// with format arguments.
TEST_F(DocExamples, LogLevelFilteringLazyProxyPerLevel) {
    Captured c;
    int value = 5;

    c.logger.set_minimum_level(LogLevel::warning);

    EXPECT_FALSE(static_cast<bool>(c.logger.trace()));
    EXPECT_FALSE(static_cast<bool>(c.logger.debug()));
    EXPECT_FALSE(static_cast<bool>(c.logger.info()));
    EXPECT_TRUE(static_cast<bool>(c.logger.warning()));
    EXPECT_TRUE(static_cast<bool>(c.logger.error()));
    EXPECT_TRUE(static_cast<bool>(c.logger.fatal()));

    if (auto log = c.logger.warning()) {
        log("value = {}", value);
    }

    ASSERT_EQ(c.sink.lines.size(), std::size_t{1});
    EXPECT_TRUE(contains(c.sink.last(), " [WARN ] value = 5"));
}

// "Macro API": with the minimum level at warning, only the last three macros
// of the list produce an event.
TEST_F(DocExamples, LogLevelFilteringLevelMacros) {
    Captured c;

    c.logger.set_minimum_level(LogLevel::warning);

    LOG_TRACE(c.logger, "filtered out");
    LOG_DEBUG(c.logger, "filtered out");
    LOG_INFO(c.logger, "filtered out");
    LOG_WARN(c.logger, "accepted");
    LOG_ERROR(c.logger, "accepted");
    LOG_FATAL(c.logger, "accepted");

    ASSERT_EQ(c.sink.lines.size(), std::size_t{3});
    EXPECT_TRUE(contains(c.sink.lines[0], " [WARN ] accepted"));
    EXPECT_TRUE(contains(c.sink.lines[1], " [ERROR] accepted"));
    EXPECT_TRUE(contains(c.sink.lines[2], " [FATAL] accepted"));
}

// "Macro API": the generic macro with a level known only at run time, and the
// logger expression evaluated exactly once.
TEST_F(DocExamples, LogLevelFilteringGenericMacroAndSingleEvaluationOfTheLogger) {
    Captured c;

    c.logger.set_minimum_level(LogLevel::warning);

    // LogLevel level = get_log_level(); LOG(logger, level, "message");
    LogLevel level = LogLevel::error;
    LOG(c.logger, level, "message");
    level = LogLevel::info;
    LOG(c.logger, level, "message");

    ASSERT_EQ(c.sink.lines.size(), std::size_t{1});
    EXPECT_TRUE(contains(c.sink.last(), " [ERROR] message"));

    // LOG_WARN(get_logger(), "message");
    int get_logger_calls = 0;
    auto get_logger = [&]() -> auto & {
        ++get_logger_calls;
        return c.logger;
    };

    LOG_WARN(get_logger(), "message");

    EXPECT_EQ(get_logger_calls, 1);
    EXPECT_EQ(c.sink.lines.size(), std::size_t{2});
}

// "Runtime reconfiguration": the minimum level can change at any time.
TEST_F(DocExamples, LogLevelFilteringRuntimeReconfiguration) {
    Captured c;

    c.logger.set_minimum_level(LogLevel::error);

    LOG_WARN(c.logger, "filtered");
    LOG_ERROR(c.logger, "accepted");
    LOG_FATAL(c.logger, "accepted");
    EXPECT_EQ(c.sink.lines.size(), std::size_t{2});

    c.logger.set_minimum_level(LogLevel::debug);

    LOG_DEBUG(c.logger, "accepted");
    EXPECT_EQ(c.sink.lines.size(), std::size_t{3});
    EXPECT_TRUE(contains(c.sink.last(), " [DEBUG] accepted"));
}

// "Runtime reconfiguration": the minimum level is a per-Logger setting.
TEST_F(DocExamples, LogLevelFilteringMinimumLevelIsPerLogger) {
    Logger logger1;
    Logger logger2;

    logger1.set_minimum_level(LogLevel::error);
    logger2.set_minimum_level(LogLevel::debug);

    EXPECT_FALSE(logger1.is_enabled(LogLevel::debug));
    EXPECT_FALSE(logger1.is_enabled(LogLevel::info));
    EXPECT_FALSE(logger1.is_enabled(LogLevel::warning));
    EXPECT_TRUE(logger1.is_enabled(LogLevel::error));
    EXPECT_TRUE(logger1.is_enabled(LogLevel::fatal));

    EXPECT_FALSE(logger2.is_enabled(LogLevel::trace));
    EXPECT_TRUE(logger2.is_enabled(LogLevel::debug));
    EXPECT_TRUE(logger2.is_enabled(LogLevel::info));
    EXPECT_TRUE(logger2.is_enabled(LogLevel::warning));
    EXPECT_TRUE(logger2.is_enabled(LogLevel::error));
    EXPECT_TRUE(logger2.is_enabled(LogLevel::fatal));
}

// ===========================================================================
// source_location.md
// ===========================================================================

using DocLogger = Logger<Target<ConsoleRenderer>, Target<JsonRenderer>>;

DocLogger *doc_logger = nullptr;
int doc_info_line = 0;
int doc_debug_line = 0;

// handle_request() of the "Complete example". The logger is reached through a
// pointer so that the function keeps the plain signature of the documentation.
void doc_handle_request() {
    const std::string request_id{"req-A"};
    const int retry_count{3};

    LOG_INFO(*doc_logger, "Request handled", extra(request_id, retry_count)); doc_info_line = __LINE__;

    if (auto log = doc_logger->debug()) {
        log("cache lookup {}", 42); doc_debug_line = __LINE__;
    }
}

// "Complete example": Short on the console, Full in the JSON file.
// Differences with the page: in-memory console, per-test file path, and the
// expected line numbers come from __LINE__ instead of being hard-coded.
TEST_F(DocExamples, SourceLocationCompleteExample) {
    std::ostringstream console_out;
    ConsoleSink console_sink{console_out};
    FileSink file_sink{path("application.log")};

    DocLogger logger{Target{ConsoleRenderer{.colors = ColorMode::Never, .source = SourceMode::Short}, console_sink},
                     Target{JsonRenderer{.source = SourceMode::Full}, file_sink}};

    doc_logger = &logger;
    doc_handle_request();
    doc_logger = nullptr;

    int warn_line = 0;
    int error_line = 0;

    logger.log(LogLevel::warning, "slow request {}", 1250, named("request_id", std::string{"req-B"})); warn_line = __LINE__;
    LOG_ERROR(logger, "Request failed"); error_line = __LINE__;

    // Console, SourceMode::Short.
    const auto console = split_lines(normalize(console_out.str()));
    ASSERT_EQ(console.size(), std::size_t{4});

    EXPECT_EQ(console[0], "[<TS>:INFO:" + where(doc_info_line) + "Request handled  request_id=req-A  retry_count=3");
    EXPECT_EQ(console[1], "[<TS>:DEBUG:" + where(doc_debug_line) + "cache lookup 42");
    EXPECT_EQ(console[2], "[<TS>:WARN:" + where(warn_line) + "slow request 1250  request_id=req-B");
    EXPECT_EQ(console[3], "[<TS>:ERROR:" + where(error_line) + "Request failed");

    // JSON file, SourceMode::Full: path as given to the compiler, line, function.
    const auto json = read_lines(path("application.log"));
    ASSERT_EQ(json.size(), std::size_t{4});

    const auto source_of = [](int line) {
        return std::string{"\"source\":{\"file\":"} + detail::json_escape_string(__FILE__) +
               ",\"line\":" + std::to_string(line) + ",\"function\":\"";
    };

    EXPECT_TRUE(json[0].starts_with(R"({"timestamp":)"));
    EXPECT_TRUE(contains(json[0], R"("level":"INFO","message":"Request handled",)" + source_of(doc_info_line)));
    EXPECT_TRUE(contains(json[0], "doc_handle_request"));
    EXPECT_TRUE(json[0].ends_with(R"(},"request_id":"req-A","retry_count":3})"));

    EXPECT_TRUE(contains(json[1], R"("level":"DEBUG","message":"cache lookup 42",)" + source_of(doc_debug_line)));
    EXPECT_TRUE(contains(json[1], "doc_handle_request"));

    // The JSON level name of a warning is "WARN".
    EXPECT_TRUE(contains(json[2], R"("level":"WARN","message":"slow request 1250",)" + source_of(warn_line)));
    EXPECT_TRUE(json[2].ends_with(R"(},"request_id":"req-B"})"));

    EXPECT_TRUE(contains(json[3], R"("level":"ERROR","message":"Request failed",)" + source_of(error_line)));
    EXPECT_TRUE(json[3].ends_with("}}"));
}

// "Console layout": SourceMode::Never keeps the historical layout, with the
// timestamp first and a padded level name.
TEST_F(DocExamples, SourceLocationNeverKeepsTheHistoricalConsoleLayout) {
    Captured c; // SourceMode::Never

    std::string request_id{"req-A"};
    int retry_count{3};

    LOG_INFO(c.logger, "Request handled", extra(request_id, retry_count));

    EXPECT_EQ(normalize(c.sink.last()), "<TS> [INFO ] Request handled  request_id=req-A  retry_count=3");
}

// "Console layout": SourceMode::Full ends the line with a quoted function= field.
TEST_F(DocExamples, SourceLocationFullQuotesTheFunctionName) {
    Captured c{SourceMode::Full};

    int line = 0;
    LOG_ERROR(c.logger, "Request failed"); line = __LINE__;

  // Full prints the path as given to the compiler (not the basename).
    const auto &s = c.sink.last();
    EXPECT_TRUE(contains(s, ":ERROR:" + std::string{__FILE__} + ":" + std::to_string(line) +
                                "] Request failed  function=\""));
    EXPECT_TRUE(s.ends_with("\""));
}

// "Every logging API captures the call site": generic API, macros, proxy.
TEST_F(DocExamples, SourceLocationEveryApiCapturesTheCallSite) {
    Captured c{SourceMode::Short};
    const std::string user = "bob";
    const int id = 7;
    int line = 0;

    // Generic API
    c.logger.log(LogLevel::info, "hello {}", 42, named("user", std::string{"bob"}), named("id", 7)); line = __LINE__;
    EXPECT_TRUE(contains(c.sink.last(), ":INFO:" + where(line) + "hello 42  user=bob  id=7"));

    // Generic macro and per-level macro
    LOG(c.logger, LogLevel::warning, "macro {}", id, extra(user)); line = __LINE__;
    EXPECT_TRUE(contains(c.sink.last(), ":WARN:" + where(line) + "macro 7  user=bob"));

    LOG_ERROR(c.logger, "level macro"); line = __LINE__;
    EXPECT_TRUE(contains(c.sink.last(), ":ERROR:" + where(line) + "level macro"));

    // Lazy proxy
    if (auto log = c.logger.info()) { log("via proxy {}", 1); line = __LINE__; }
    EXPECT_TRUE(contains(c.sink.last(), ":INFO:" + where(line) + "via proxy 1"));
}

// "Wrappers": forwarding the message through a parameter pack reports the
// location of the helper; declaring a Located parameter reports the caller's.
template <typename LoggerT, typename... Args> void pack_forwarding_log(LoggerT &lg, int &helper_line, Args &&...args) {
    lg.log(LogLevel::info, std::forward<Args>(args)...); helper_line = __LINE__;
}

template <typename LoggerT, typename... Args> void located_log(LoggerT &lg, Located message, Args &&...args) {
    lg.log(LogLevel::info, message, std::forward<Args>(args)...);
}

TEST_F(DocExamples, SourceLocationWrappers) {
    Captured c{SourceMode::Short};

    int helper_line = 0;
    int caller_line = 0;

    pack_forwarding_log(c.logger, helper_line, "through a pack"); caller_line = __LINE__;

    EXPECT_TRUE(contains(c.sink.last(), where(helper_line) + "through a pack"));
    EXPECT_FALSE(contains(c.sink.last(), where(caller_line)));

    located_log(c.logger, "through Located"); caller_line = __LINE__;

    EXPECT_TRUE(contains(c.sink.last(), where(caller_line) + "through Located"));
}

// "Events without a call site": the default layout is kept, even in Full.
TEST_F(DocExamples, SourceLocationEventWithoutCallSite) {
    const LogEvent ev{.level = LogLevel::info, .timestamp = std::chrono::system_clock::now(), .message = "m"};

    const auto text = ConsoleRenderer{.colors = ColorMode::Never, .source = SourceMode::Full}.render(ev, make_context());
    EXPECT_TRUE(contains(text, " [INFO ] m"));
    EXPECT_FALSE(contains(text, "function="));

    const auto json = JsonRenderer{.source = SourceMode::Full}.render(ev, make_context());
    EXPECT_FALSE(contains(json, "\"source\""));
}

// "Testing the location in your own code": the two tests given on the page.
TEST_F(DocExamples, SourceLocationTestingYourOwnCode) {
    {
        CaptureSink sink;
        Logger lg{Target{ConsoleRenderer{.colors = ColorMode::Never, .source = SourceMode::Short}, sink}};

        int line = 0;
        LOG_ERROR(lg, "level macro"); line = __LINE__; // same line on purpose

        const auto basename = detail::source_basename(__FILE__);
        EXPECT_TRUE(
            contains(sink.last(), ":ERROR:" + std::string{basename} + ":" + std::to_string(line) + "] level macro"));
    }
    {
        CaptureSink sink;
        Logger lg{Target{JsonRenderer{.source = SourceMode::Short}, sink}};

        lg.log(LogLevel::error, "json short", named("user", std::string{"bob"}));

        const auto &s = sink.last();
        EXPECT_FALSE(contains(s, "\"function\""));
        EXPECT_LT(s.find("\"message\""), s.find("\"source\""));
        EXPECT_LT(s.find("\"source\""), s.find("\"user\""));
    }
}

} // namespace
