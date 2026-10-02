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

#include "logger.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

using namespace logchain;

namespace {

namespace fs = std::filesystem;

class FileSinkTest : public ::testing::Test {
  protected:
    fs::path test_dir;

    void SetUp() override {
        test_dir = fs::temp_directory_path() / "logchain_file_sink_tests";

        std::error_code ec;
        fs::remove_all(test_dir, ec);
        ASSERT_FALSE(ec);

        fs::create_directories(test_dir, ec);
        ASSERT_FALSE(ec);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(test_dir, ec);
        ASSERT_FALSE(ec);
    }

    fs::path path(std::string_view name) const { return test_dir / std::string(name); }

    static std::vector<std::string> read_lines(const fs::path &file) {
        std::ifstream in(file);

        EXPECT_TRUE(in.is_open());

        std::vector<std::string> lines;
        std::string line;

        while (std::getline(in, line)) {
            lines.push_back(line);
        }

        return lines;
    }

    static std::string read_file(const fs::path &file) {
        std::ifstream in(file);

        EXPECT_TRUE(in.is_open());

        return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    }
};

// ===========================================================================
// Basic behaviour
// ===========================================================================

TEST_F(FileSinkTest, writes_log_messages) {
    const auto log_path = path("application.log");

    {
        FileSink sink(log_path);
        Logger logger(target(sink));

        LOG_INFO(logger, "first message");
        LOG_INFO(logger, "second message");
    }

    const auto lines = read_lines(log_path);

    ASSERT_EQ(lines.size(), 2u);
    EXPECT_NE(lines[0].find("\"message\":\"first message\""), std::string::npos);
    EXPECT_NE(lines[1].find("\"message\":\"second message\""), std::string::npos);
}

// ===========================================================================
// Two independent FileSink instances
// ===========================================================================

TEST_F(FileSinkTest, different_file_sinks_are_independent) {
    const auto path_a = path("application-a.log");
    const auto path_b = path("application-b.log");

    {
        FileSink sink_a(path_a);
        FileSink sink_b(path_b);

        Logger logger_a(target(sink_a));
        Logger logger_b(target(sink_b));

        LOG_INFO(logger_a, "message A");
        LOG_INFO(logger_b, "message B");
    }

    const auto lines_a = read_lines(path_a);
    const auto lines_b = read_lines(path_b);

    ASSERT_EQ(lines_a.size(), 1u);
    ASSERT_EQ(lines_b.size(), 1u);

    EXPECT_NE(lines_a[0].find("\"message\":\"message A\""), std::string::npos);

    EXPECT_NE(lines_b[0].find("\"message\":\"message B\""), std::string::npos);
}

// ===========================================================================
// One FileSink shared by multiple Logger instances
// ===========================================================================

TEST_F(FileSinkTest, same_file_sink_can_be_shared_by_multiple_loggers) {
    const auto log_path = path("application.log");

    {
        FileSink sink(log_path);

        Logger logger_a(target(sink));
        Logger logger_b(target(sink));

        LOG_INFO(logger_a, "message A");
        LOG_INFO(logger_b, "message B");
    }

    const auto lines = read_lines(log_path);

    ASSERT_EQ(lines.size(), 2u);

    EXPECT_NE(lines[0].find("\"message\":\"message A\""), std::string::npos);

    EXPECT_NE(lines[1].find("\"message\":\"message B\""), std::string::npos);
}

// ===========================================================================
// Duplicate FileSink ownership
// ===========================================================================

TEST_F(FileSinkTest, duplicate_file_path_is_rejected) {
    const auto log_path = path("application.log");

    FileSink first(log_path);

    EXPECT_THROW({ FileSink second(log_path); }, std::runtime_error);
}

TEST_F(FileSinkTest, equivalent_file_paths_are_rejected) {
    const auto log_path = path("application.log");
    const auto equivalent_path = test_dir / "." / "application.log";

    FileSink first(log_path);

    EXPECT_THROW({ FileSink second(equivalent_path); }, std::runtime_error);
}

TEST_F(FileSinkTest, file_path_becomes_available_after_sink_destruction) {
    const auto log_path = path("application.log");

    {
        FileSink first(log_path);
    }

    EXPECT_NO_THROW({ FileSink second(log_path); });
}

// ===========================================================================
// Configuration validation
// ===========================================================================

TEST_F(FileSinkTest, zero_max_files_is_rejected) {
    EXPECT_THROW(
        { FileSink sink(FileSinkSettings{.path = path("application.log"), .max_files = 0, .max_file_size = 100}); },
        std::invalid_argument);
}

TEST_F(FileSinkTest, zero_max_file_size_is_rejected) {
    EXPECT_THROW(
        { FileSink sink(FileSinkSettings{.path = path("application.log"), .max_files = 5, .max_file_size = 0}); },
        std::invalid_argument);
}

// ===========================================================================
// Rotation
// ===========================================================================

TEST_F(FileSinkTest, rotates_when_file_size_limit_is_reached) {
    const auto log_path = path("application.log");

    {
        FileSink sink(FileSinkSettings{.path = log_path, .max_files = 3, .max_file_size = 20});

        Logger logger(target(sink));

        LOG_INFO(logger, "111111");
        LOG_INFO(logger, "222222");
        LOG_INFO(logger, "333333");
    }

    EXPECT_TRUE(fs::exists(log_path));
    EXPECT_TRUE(fs::exists(log_path.string() + ".1"));
    EXPECT_TRUE(fs::exists(log_path.string() + ".2"));
}

// ===========================================================================
// Rotation keeps at most max_files files
// ===========================================================================

TEST_F(FileSinkTest, rotation_removes_oldest_file) {
    const auto log_path = path("application.log");

    {
        FileSink sink(FileSinkSettings{.path = log_path, .max_files = 3, .max_file_size = 20});

        Logger logger(target(sink));

        for (int i = 0; i < 10; ++i) {
            LOG_INFO(logger, "message {}", i);
        }
    }

    EXPECT_TRUE(fs::exists(log_path));
    EXPECT_TRUE(fs::exists(log_path.string() + ".1"));
    EXPECT_TRUE(fs::exists(log_path.string() + ".2"));
    EXPECT_FALSE(fs::exists(log_path.string() + ".3"));
}

// ===========================================================================
// max_files == 1 means no archive
// ===========================================================================

TEST_F(FileSinkTest, max_files_one_keeps_only_current_file) {
    const auto log_path = path("application.log");

    {
        FileSink sink(FileSinkSettings{.path = log_path, .max_files = 1, .max_file_size = 20});

        Logger logger(target(sink));

        LOG_INFO(logger, "first");
        LOG_INFO(logger, "second");
        LOG_INFO(logger, "third");
    }

    EXPECT_TRUE(fs::exists(log_path));
    EXPECT_FALSE(fs::exists(log_path.string() + ".1"));
}

// ===========================================================================
// Existing file
// ===========================================================================

TEST_F(FileSinkTest, existing_file_size_is_accounted_for) {
    const auto log_path = path("application.log");

    {
        std::ofstream out(log_path);
        ASSERT_TRUE(out.is_open());
        out << "1234567890\n";
    }

    const auto before = fs::file_size(log_path);
    ASSERT_EQ(before, 11u);

    {
        FileSink sink(FileSinkSettings{.path = log_path, .max_files = 3, .max_file_size = 20});

        Logger logger(target(sink));

        LOG_INFO(logger, "abcdefghij");
    }

    EXPECT_TRUE(fs::exists(log_path));
    EXPECT_TRUE(fs::exists(log_path.string() + ".1"));
}

// ===========================================================================
// Large individual message
// ===========================================================================

TEST_F(FileSinkTest, message_larger_than_limit_is_written_without_empty_rotation) {
    const auto log_path = path("application.log");

    {
        FileSink sink(FileSinkSettings{.path = log_path, .max_files = 3, .max_file_size = 10});

        Logger logger(target(sink));

        LOG_INFO(logger, "this message is larger than ten bytes");
    }

    EXPECT_TRUE(fs::exists(log_path));
    EXPECT_FALSE(fs::exists(log_path.string() + ".1"));

    const auto lines = read_lines(log_path);

    ASSERT_EQ(lines.size(), 1u);
    EXPECT_NE(lines[0].find("this message is larger than ten bytes"), std::string::npos);
}

} // namespace
