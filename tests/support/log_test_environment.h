#pragma once

#include "log/log.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>

// Registered once via ::testing::AddGlobalTestEnvironment() in main.cpp's
// "-t" handling, before RUN_ALL_TESTS() runs any TEST() - without this,
// any FLOG_*() call reached during a test crashes: channelLogger()
// (log/log.h) hands back a LoggerBase that init() never populated, and
// quill's own should_log_statement() dereferences it unconditionally.
// Confirmed via gdb backtrace: SIGSEGV inside
// quill::detail::LoggerBase::get_log_level(), reached through
// FlatComboItemDelegate's FLOG_DEBUG call (widgets/flat_combobox.cpp) -
// the real app always calls familiar::log::init() early in main() (before
// MainWindow is constructed), but "familiar -t" skips straight past that
// line on its way to RUN_ALL_TESTS(), so tests need their own call.
//
// filePath redirected to a temp file, same reasoning as
// SettingsTestEnvironment (tests/support/settings_test_environment.h) -
// the default (AppLocalDataLocation/<appName>.log) would otherwise write
// into the real app's actual log directory on every test run.
class LogTestEnvironment : public ::testing::Environment
{
public:
    void SetUp() override
    {
        familiar::log::Options options;
        options.filePath = QDir::temp().filePath(
            QStringLiteral("familiar_test_%1.log")
                .arg(QCoreApplication::applicationPid()));
        // Real log lines interleaved with gtest's own [ RUN ]/[ OK ]
        // console output otherwise, for no benefit - the file sink still
        // captures everything for anyone who actually needs to inspect
        // it (logFilePath() below).
        options.console = false;
        familiar::log::init(options);
    }

    void TearDown() override { familiar::log::shutdown(); }
};
