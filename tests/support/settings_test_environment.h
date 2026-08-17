#pragma once

#include "core/settings.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>

// Registered once via ::testing::AddGlobalTestEnvironment() in main.cpp's
// "-t" handling, before RUN_ALL_TESTS() runs any TEST(). SettingsHandler
// (core/settingshandler.cpp) is a lazy singleton that reads
// CommandlineArgs::instance().settingsFile() the FIRST time anything
// calls SettingsHandler::getInstance(), falling back to the user's real
// ~/.config/familiar/settings.json when that's empty - and the path
// sticks for the rest of the process once read. Redirecting it here,
// before any TEST() gets a chance to be the first caller, keeps the
// whole suite from ever touching the real file regardless of which test
// happens to run first.
class SettingsTestEnvironment : public ::testing::Environment
{
public:
    void SetUp() override
    {
        path_ = QDir::temp().filePath(
            QStringLiteral("familiar_test_settings_%1.json")
                .arg(QCoreApplication::applicationPid()));
        CommandlineArgs::instance().parse({QStringLiteral("familiar"),
                                           QStringLiteral("--settings"),
                                           path_});
    }

    void TearDown() override { QFile::remove(path_); }

private:
    QString path_;
};
