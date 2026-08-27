#include "log/log.h"

#include <gtest/gtest.h>

#include <algorithm>

#include <QList>

// familiar::log::init() has already run by the time any TEST() body
// below executes - tests/support/log_test_environment.h's
// LogTestEnvironment runs it first (redirected to a throwaway temp file,
// console output off), registered before RUN_ALL_TESTS() in main.cpp -
// see that file's own comment for why (a real SIGSEGV was found and
// fixed by adding it). Safe to exercise the rest of this facade
// directly from here without a second init()/shutdown() pair.

using namespace familiar::log;

TEST(LevelFromNameTest, ParsesEachRecognizedLevelCaseInsensitively)
{
    EXPECT_EQ(levelFromName(QStringLiteral("trace")), Level::Trace);
    EXPECT_EQ(levelFromName(QStringLiteral("DEBUG")), Level::Debug);
    EXPECT_EQ(levelFromName(QStringLiteral("Info")), Level::Info);
    EXPECT_EQ(levelFromName(QStringLiteral("warning")), Level::Warning);
    EXPECT_EQ(levelFromName(QStringLiteral("WARN")), Level::Warning);
    EXPECT_EQ(levelFromName(QStringLiteral("error")), Level::Error);
    EXPECT_EQ(levelFromName(QStringLiteral("critical")), Level::Critical);
    EXPECT_EQ(levelFromName(QStringLiteral("FATAL")), Level::Critical);
}

TEST(LevelFromNameTest, TrimsWhitespaceAroundTheName)
{
    EXPECT_EQ(levelFromName(QStringLiteral("  info  ")), Level::Info);
}

TEST(LevelFromNameTest, UnrecognizedNameFallsBackToGivenDefault)
{
    EXPECT_EQ(levelFromName(QStringLiteral("nonsense"), Level::Critical),
             Level::Critical);
    // Own default (Level::Info) when the caller doesn't specify one.
    EXPECT_EQ(levelFromName(QStringLiteral("nonsense")), Level::Info);
}

TEST(ToQuillLevelTest, MapsEveryFamiliarLevelToItsQuillCounterpart)
{
    EXPECT_EQ(detail::toQuillLevel(Level::Trace), quill::LogLevel::TraceL1);
    EXPECT_EQ(detail::toQuillLevel(Level::Debug), quill::LogLevel::Debug);
    EXPECT_EQ(detail::toQuillLevel(Level::Info), quill::LogLevel::Info);
    EXPECT_EQ(detail::toQuillLevel(Level::Warning), quill::LogLevel::Warning);
    EXPECT_EQ(detail::toQuillLevel(Level::Error), quill::LogLevel::Error);
    EXPECT_EQ(detail::toQuillLevel(Level::Critical),
             quill::LogLevel::Critical);
}

TEST(LogFacadeTest, LogFilePathIsResolvedAfterInit)
{
    // LogTestEnvironment redirects this to a temp file - the point here
    // is only that init() actually resolved *something*, not that it
    // matches a specific real location.
    EXPECT_FALSE(logFilePath().isEmpty());
}

TEST(LogFacadeTest, RingSinkIsAvailableAfterInit)
{
    ASSERT_NE(ringSink(), nullptr);
}

TEST(LogFacadeTest, SetChannelLevelChangesTheLoggerLevel)
{
    quill::Logger* logger = channelLogger(Ch::UI);
    ASSERT_NE(logger, nullptr);
    const quill::LogLevel original = logger->get_log_level();

    setChannelLevel(Ch::UI, Level::Error);
    EXPECT_EQ(logger->get_log_level(), quill::LogLevel::Error);

    // Restore - every other test in this binary logs through Ch::UI too,
    // and a level left at Error would silently swallow their Info/Debug
    // lines for the rest of this test binary's run.
    logger->set_log_level(original);
}

TEST(LogFacadeTest, FlogInfoIsCapturedByTheRingSinkAfterFlush)
{
    ASSERT_NE(ringSink(), nullptr);

    FLOG_INFO(Ch::UI, "ring sink marker RSMARKER8842");
    // quill is async - the backend thread only picks up a queued record
    // once flush_log() has actually waited for it (blocking by default),
    // otherwise reading entries() below would be racing the backend.
    channelLogger(Ch::UI)->flush_log();

    const QList<RingSink::Entry> lines = ringSink()->entries();
    const bool found = std::any_of(
        lines.begin(), lines.end(), [](const RingSink::Entry& e) {
            return e.line.contains(QStringLiteral("RSMARKER8842"));
        });
    EXPECT_TRUE(found);
}

TEST(LogFacadeTest, FlogTimerLogsElapsedTimeAtScopeExit)
{
    { FLOG_TIMER(Ch::UI, "flog_timer marker TMMARKER7331"); }
    channelLogger(Ch::UI)->flush_log();

    const QList<RingSink::Entry> lines = ringSink()->entries();
    const bool found = std::any_of(
        lines.begin(), lines.end(), [](const RingSink::Entry& e) {
            return e.line.contains(QStringLiteral("TMMARKER7331"))
                   && e.line.contains(QStringLiteral("took"));
        });
    EXPECT_TRUE(found);
}

TEST(DebugStringTest, CapturesQDebugStreamOutputForATypeWithNoFormatter)
{
    // QList<int> has no fmtquill::formatter of its own - debugString()
    // exists precisely to bridge cases like this through Qt's own
    // operator<<(QDebug, ...) instead (see debugString()'s own comment).
    // Checking substrings, not an exact string, since QDebug's own
    // punctuation/spacing around a container isn't this function's
    // contract to preserve.
    const QString result = debugString(QList<int>{1, 2, 3});
    EXPECT_TRUE(result.contains(QStringLiteral("1")));
    EXPECT_TRUE(result.contains(QStringLiteral("2")));
    EXPECT_TRUE(result.contains(QStringLiteral("3")));
}
