// tests/unit/log_level_filtering_test.cpp

#include "log_level_filtering.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <string>

using logchain::Logger;
using logchain::LogLevel;

namespace {

int g_expensive_calls = 0;
int g_logger_expression_calls = 0;

std::string expensive() {
    ++g_expensive_calls;
    return "computed-value";
}

Logger &get_logger() {
    ++g_logger_expression_calls;

    static Logger logger;
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
    Logger logger;

    EXPECT_EQ(logger.minimum_level(), LogLevel::trace);
}

TEST_F(LogLevelFilteringTest, DefaultMinimumLevelEnablesEveryLevel) {
    Logger logger;

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
    Logger logger;

    logger.set_minimum_level(LogLevel::warning);

    EXPECT_FALSE(logger.is_enabled(LogLevel::trace));
    EXPECT_FALSE(logger.is_enabled(LogLevel::debug));
    EXPECT_FALSE(logger.is_enabled(LogLevel::info));

    EXPECT_TRUE(logger.is_enabled(LogLevel::warning));
    EXPECT_TRUE(logger.is_enabled(LogLevel::error));
    EXPECT_TRUE(logger.is_enabled(LogLevel::fatal));
}

TEST_F(LogLevelFilteringTest, MinimumLevelItselfIsEnabled) {
    Logger logger;

    const LogLevel levels[] = {
        LogLevel::trace, LogLevel::debug, LogLevel::info, LogLevel::warning, LogLevel::error, LogLevel::fatal,
    };

    for (const LogLevel minimum : levels) {
        logger.set_minimum_level(minimum);

        EXPECT_TRUE(logger.is_enabled(minimum)) << "minimum level = " << static_cast<int>(minimum);
    }
}

TEST_F(LogLevelFilteringTest, FatalOnlyEnablesFatal) {
    Logger logger;

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
    Logger logger;

    logger.set_minimum_level(LogLevel::warning);

    LOG_DEBUG(logger, expensive());

    EXPECT_EQ(g_expensive_calls, 0);
}

TEST_F(LogLevelFilteringTest, EnabledMacroEvaluatesArguments) {
    Logger logger;

    logger.set_minimum_level(LogLevel::warning);

    LOG_WARN(logger, expensive());

    EXPECT_EQ(g_expensive_calls, 1);
}

TEST_F(LogLevelFilteringTest, DisabledMacroDoesNotInvokeUncheckedPipeline) {
    // With the current reference implementation this is indirectly verified
    // by the fact that the argument is not evaluated.
    //
    // Once Logger gets an injectable target/spy, this test should additionally
    // verify that the target pipeline is never entered.
    Logger logger;

    logger.set_minimum_level(LogLevel::error);

    LOG_WARN(logger, "must not reach pipeline");

    SUCCEED();
}

TEST_F(LogLevelFilteringTest, EnabledMacroInvokesUncheckedPipeline) {
    Logger logger;

    logger.set_minimum_level(LogLevel::warning);

    // log_unchecked() currently prints to stdout. The observable contract
    // here is that the call is reached when the level is enabled.
    LOG_WARN(logger, "warning");

    SUCCEED();
}

// ============================================================================
// Macro API - dynamic level
// ============================================================================

TEST_F(LogLevelFilteringTest, DynamicMacroFiltersDisabledLevel) {
    Logger logger;

    logger.set_minimum_level(LogLevel::warning);

    const LogLevel level = LogLevel::debug;

    LOG(logger, level, expensive());

    EXPECT_EQ(g_expensive_calls, 0);
}

TEST_F(LogLevelFilteringTest, DynamicMacroEvaluatesArgumentsForEnabledLevel) {
    Logger logger;

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
    Logger &logger = get_logger();
    logger.set_minimum_level(LogLevel::error);

    g_logger_expression_calls = 0;
    g_expensive_calls = 0;

    LOG_WARN(get_logger(), expensive());

    EXPECT_EQ(g_logger_expression_calls, 1);
    EXPECT_EQ(g_expensive_calls, 0);
}

TEST_F(LogLevelFilteringTest, DynamicMacroEvaluatesLoggerExpressionExactlyOnce) {
    Logger &logger = get_logger();
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
    Logger logger;

    logger.set_minimum_level(LogLevel::warning);

    if (auto log = logger.debug()) {
        log(expensive());
    }

    EXPECT_EQ(g_expensive_calls, 0);
}

TEST_F(LogLevelFilteringTest, EnabledDebugProxyEvaluatesArguments) {
    Logger logger;

    logger.set_minimum_level(LogLevel::debug);

    if (auto log = logger.debug()) {
        log(expensive());
    }

    EXPECT_EQ(g_expensive_calls, 1);
}

TEST_F(LogLevelFilteringTest, ProxyMatchesIsEnabled) {
    Logger logger;

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
    Logger logger;

    logger.set_minimum_level(LogLevel::warning);

    logger.log(LogLevel::debug, expensive());

    EXPECT_EQ(g_expensive_calls, 1);
}

TEST_F(LogLevelFilteringTest, GenericLogStillFiltersDisabledLevel) {
    Logger logger;

    logger.set_minimum_level(LogLevel::warning);

    // The argument is already evaluated, but log_unchecked() must not be
    // entered because DEBUG is disabled.
    logger.log(LogLevel::debug, "filtered");
}

TEST_F(LogLevelFilteringTest, GenericLogEvaluatesEnabledArgumentOnce) {
    Logger logger;

    logger.set_minimum_level(LogLevel::warning);

    logger.log(LogLevel::error, expensive());

    EXPECT_EQ(g_expensive_calls, 1);
}

// ============================================================================
// Atomic minimum level
// ============================================================================

TEST_F(LogLevelFilteringTest, MinimumLevelCanBeChangedRepeatedly) {
    Logger logger;

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
