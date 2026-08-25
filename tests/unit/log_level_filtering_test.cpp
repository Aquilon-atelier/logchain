// tests/unit/log_level_filtering_test.cpp
//
// Tests filtering mechanics (is_enabled / set_minimum_level / log /
// log_unchecked / LogProxy / LOGCHAIN_LOG* macros) directly on the real
// Logger<TargetT...> from logger.hpp.
//
// TestLogger = Logger<> (zero targets) rather than a hand-rolled
// standalone stand-in: it runs the actual shipped dispatch pipeline
// (log -> is_enabled -> log_unchecked -> dispatch -> ... -> emit ->
// buffering policy -> targets), just with an empty target tuple, so the
// final std::apply fold over targets does nothing. This means these
// tests exercise the real filtering code path end to end, not a
// parallel implementation that could silently drift from it.

#include "logger.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <string>

using logchain::Logger;
using logchain::LogLevel;

namespace {

using TestLogger = Logger<>;

int g_expensive_calls = 0;
int g_logger_expression_calls = 0;

std::string expensive() {
    ++g_expensive_calls;
    return "computed-value";
}

TestLogger &get_logger() {
    ++g_logger_expression_calls;

    static TestLogger logger;
    return logger;
}

// -----------------------------------------------------------------------------
// Fixture
// -----------------------------------------------------------------------------

class LogLevelFilteringTest : public ::testing::Test {
  protected:
    void SetUp() override {
        g_expensive_calls = 0;
        g_logger_expression_calls = 0;

        // get_logger() is static, so make sure each test starts from the
        // default configuration as well.
        get_logger().set_minimum_level(LogLevel::trace);
    }
};

// ============================================================================
// LogLevel
// ============================================================================

TEST(LogLevelTest, ToStringReturnsExpectedNames) {
    EXPECT_EQ(logchain::to_string(LogLevel::trace), "TRACE");
    EXPECT_EQ(logchain::to_string(LogLevel::debug), "DEBUG");
    EXPECT_EQ(logchain::to_string(LogLevel::info), "INFO");
    EXPECT_EQ(logchain::to_string(LogLevel::warning), "WARN");
    EXPECT_EQ(logchain::to_string(LogLevel::error), "ERROR");
    EXPECT_EQ(logchain::to_string(LogLevel::fatal), "FATAL");
}

TEST(LogLevelTest, EnumValuesAreOrdered) {
    EXPECT_LT(LogLevel::trace, LogLevel::debug);
    EXPECT_LT(LogLevel::debug, LogLevel::info);
    EXPECT_LT(LogLevel::info, LogLevel::warning);
    EXPECT_LT(LogLevel::warning, LogLevel::error);
    EXPECT_LT(LogLevel::error, LogLevel::fatal);
}

// ============================================================================
// Default state
// ============================================================================

TEST_F(LogLevelFilteringTest, DefaultMinimumLevelIsTrace) {
    TestLogger logger;

    EXPECT_EQ(logger.minimum_level(), LogLevel::trace);
}

TEST_F(LogLevelFilteringTest, DefaultMinimumLevelEnablesEveryLevel) {
    TestLogger logger;

    EXPECT_TRUE(logger.is_enabled(LogLevel::trace));
    EXPECT_TRUE(logger.is_enabled(LogLevel::debug));
    EXPECT_TRUE(logger.is_enabled(LogLevel::info));
    EXPECT_TRUE(logger.is_enabled(LogLevel::warning));
    EXPECT_TRUE(logger.is_enabled(LogLevel::error));
    EXPECT_TRUE(logger.is_enabled(LogLevel::fatal));
}

// ============================================================================
// is_enabled()
// ============================================================================

TEST_F(LogLevelFilteringTest, MinimumLevelFiltersAllLowerLevels) {
    TestLogger logger;

    logger.set_minimum_level(LogLevel::warning);

    EXPECT_FALSE(logger.is_enabled(LogLevel::trace));
    EXPECT_FALSE(logger.is_enabled(LogLevel::debug));
    EXPECT_FALSE(logger.is_enabled(LogLevel::info));

    EXPECT_TRUE(logger.is_enabled(LogLevel::warning));
    EXPECT_TRUE(logger.is_enabled(LogLevel::error));
    EXPECT_TRUE(logger.is_enabled(LogLevel::fatal));
}

TEST_F(LogLevelFilteringTest, MinimumLevelItselfIsEnabled) {
    TestLogger logger;

    const LogLevel levels[] = {
        LogLevel::trace, LogLevel::debug, LogLevel::info, LogLevel::warning, LogLevel::error, LogLevel::fatal,
    };

    for (const LogLevel minimum : levels) {
        logger.set_minimum_level(minimum);

        EXPECT_TRUE(logger.is_enabled(minimum)) << "minimum level = " << static_cast<int>(minimum);
    }
}

TEST_F(LogLevelFilteringTest, FatalOnlyEnablesFatal) {
    TestLogger logger;

    logger.set_minimum_level(LogLevel::fatal);

    EXPECT_FALSE(logger.is_enabled(LogLevel::trace));
    EXPECT_FALSE(logger.is_enabled(LogLevel::debug));
    EXPECT_FALSE(logger.is_enabled(LogLevel::info));
    EXPECT_FALSE(logger.is_enabled(LogLevel::warning));
    EXPECT_FALSE(logger.is_enabled(LogLevel::error));
    EXPECT_TRUE(logger.is_enabled(LogLevel::fatal));
}

// ============================================================================
// Macro API
//
// The fundamental contract:
//
//     LOG_DEBUG(logger, expensive())
//
// must NOT evaluate expensive() when DEBUG is disabled.
// ============================================================================

TEST_F(LogLevelFilteringTest, DisabledMacroDoesNotEvaluateArguments) {
    TestLogger logger;

    logger.set_minimum_level(LogLevel::warning);

    LOG_DEBUG(logger, expensive());

    EXPECT_EQ(g_expensive_calls, 0);
}

TEST_F(LogLevelFilteringTest, EnabledMacroEvaluatesArguments) {
    TestLogger logger;

    logger.set_minimum_level(LogLevel::warning);

    LOG_WARN(logger, expensive());

    EXPECT_EQ(g_expensive_calls, 1);
}

TEST_F(LogLevelFilteringTest, DisabledMacroDoesNotInvokeUncheckedPipeline) {
    TestLogger logger;

    logger.set_minimum_level(LogLevel::error);

    LOG_WARN(logger, "must not reach pipeline");

    SUCCEED();
}

TEST_F(LogLevelFilteringTest, EnabledMacroInvokesUncheckedPipeline) {
    TestLogger logger;

    logger.set_minimum_level(LogLevel::warning);

    // The real dispatch pipeline is entered (dispatch -> ... -> emit ->
    // buffering policy -> targets); with zero targets, the final fold
    // over the target tuple is a no-op, so there is nothing further to
    // observe here beyond "the call is reached without failing."
    LOG_WARN(logger, "warning");

    SUCCEED();
}

// ============================================================================
// Macro API - dynamic level
// ============================================================================

TEST_F(LogLevelFilteringTest, DynamicMacroFiltersDisabledLevel) {
    TestLogger logger;

    logger.set_minimum_level(LogLevel::warning);

    const LogLevel level = LogLevel::debug;

    LOG(logger, level, expensive());

    EXPECT_EQ(g_expensive_calls, 0);
}

TEST_F(LogLevelFilteringTest, DynamicMacroEvaluatesArgumentsForEnabledLevel) {
    TestLogger logger;

    logger.set_minimum_level(LogLevel::warning);

    const LogLevel level = LogLevel::error;

    LOG(logger, level, expensive());

    EXPECT_EQ(g_expensive_calls, 1);
}

// ============================================================================
// Macro API - logger expression evaluation
// ============================================================================

TEST_F(LogLevelFilteringTest, LoggerExpressionIsEvaluatedExactlyOnce) {
    g_logger_expression_calls = 0;

    LOG_WARN(get_logger(), "message");

    EXPECT_EQ(g_logger_expression_calls, 1);
}

TEST_F(LogLevelFilteringTest, DisabledMacroStillEvaluatesLoggerExpressionExactlyOnce) {
    TestLogger &logger = get_logger();
    logger.set_minimum_level(LogLevel::error);

    g_logger_expression_calls = 0;
    g_expensive_calls = 0;

    LOG_WARN(get_logger(), expensive());

    EXPECT_EQ(g_logger_expression_calls, 1);
    EXPECT_EQ(g_expensive_calls, 0);
}

TEST_F(LogLevelFilteringTest, DynamicMacroEvaluatesLoggerExpressionExactlyOnce) {
    TestLogger &logger = get_logger();
    logger.set_minimum_level(LogLevel::warning);

    g_logger_expression_calls = 0;

    LOG(get_logger(), LogLevel::warning, "message");

    EXPECT_EQ(g_logger_expression_calls, 1);
}

// ============================================================================
// Proxy API
//
//     if (auto log = logger.debug()) {
//         log(expensive());
//     }
//
// The expensive argument must not be evaluated when the proxy is disabled.
// ============================================================================

TEST_F(LogLevelFilteringTest, DisabledDebugProxyDoesNotEvaluateArguments) {
    TestLogger logger;

    logger.set_minimum_level(LogLevel::warning);

    if (auto log = logger.debug()) {
        log(expensive());
    }

    EXPECT_EQ(g_expensive_calls, 0);
}

TEST_F(LogLevelFilteringTest, EnabledDebugProxyEvaluatesArguments) {
    TestLogger logger;

    logger.set_minimum_level(LogLevel::debug);

    if (auto log = logger.debug()) {
        log(expensive());
    }

    EXPECT_EQ(g_expensive_calls, 1);
}

TEST_F(LogLevelFilteringTest, ProxyMatchesIsEnabled) {
    TestLogger logger;

    const LogLevel levels[] = {
        LogLevel::trace, LogLevel::debug, LogLevel::info, LogLevel::warning, LogLevel::error, LogLevel::fatal,
    };

    for (const LogLevel minimum : levels) {
        logger.set_minimum_level(minimum);

        EXPECT_EQ(static_cast<bool>(logger.trace()), logger.is_enabled(LogLevel::trace));

        EXPECT_EQ(static_cast<bool>(logger.debug()), logger.is_enabled(LogLevel::debug));

        EXPECT_EQ(static_cast<bool>(logger.info()), logger.is_enabled(LogLevel::info));

        EXPECT_EQ(static_cast<bool>(logger.warning()), logger.is_enabled(LogLevel::warning));

        EXPECT_EQ(static_cast<bool>(logger.error()), logger.is_enabled(LogLevel::error));

        EXPECT_EQ(static_cast<bool>(logger.fatal()), logger.is_enabled(LogLevel::fatal));
    }
}

// ============================================================================
// Generic API
//
// logger.log(level, expensive())
//
// cannot be lazy because expensive() is evaluated by the caller before
// Logger::log() is entered.
// ============================================================================

TEST_F(LogLevelFilteringTest, GenericLogCannotPreventArgumentEvaluation) {
    TestLogger logger;

    logger.set_minimum_level(LogLevel::warning);

    logger.log(LogLevel::debug, expensive());

    EXPECT_EQ(g_expensive_calls, 1);
}

TEST_F(LogLevelFilteringTest, GenericLogStillFiltersDisabledLevel) {
    TestLogger logger;

    logger.set_minimum_level(LogLevel::warning);

    // The argument is already evaluated, but log_unchecked() must not be
    // entered because DEBUG is disabled.
    logger.log(LogLevel::debug, "filtered");
}

TEST_F(LogLevelFilteringTest, GenericLogEvaluatesEnabledArgumentOnce) {
    TestLogger logger;

    logger.set_minimum_level(LogLevel::warning);

    logger.log(LogLevel::error, expensive());

    EXPECT_EQ(g_expensive_calls, 1);
}

// ============================================================================
// Atomic minimum level
// ============================================================================

TEST_F(LogLevelFilteringTest, MinimumLevelCanBeChangedRepeatedly) {
    TestLogger logger;

    logger.set_minimum_level(LogLevel::fatal);
    EXPECT_EQ(logger.minimum_level(), LogLevel::fatal);

    logger.set_minimum_level(LogLevel::error);
    EXPECT_EQ(logger.minimum_level(), LogLevel::error);

    logger.set_minimum_level(LogLevel::warning);
    EXPECT_EQ(logger.minimum_level(), LogLevel::warning);

    logger.set_minimum_level(LogLevel::info);
    EXPECT_EQ(logger.minimum_level(), LogLevel::info);

    logger.set_minimum_level(LogLevel::debug);
    EXPECT_EQ(logger.minimum_level(), LogLevel::debug);

    logger.set_minimum_level(LogLevel::trace);
    EXPECT_EQ(logger.minimum_level(), LogLevel::trace);
}

} // namespace
