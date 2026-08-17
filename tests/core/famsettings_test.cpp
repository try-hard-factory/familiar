#include "core/settings.h"
#include "core/settingshandler.h"

#include <gtest/gtest.h>

#include <QDir>

// FamSettings is a thin value-typed facade (core/settings.h) - a fresh
// one is constructed per test the same way real call sites do
// ("FamSettings settings; settings.valueOrDefault(key)"), it holds no
// state of its own. Actual storage is SettingsHandler's singleton, which
// tests/support/settings_test_environment.h has already redirected to a
// throwaway temp file (registered in main.cpp before RUN_ALL_TESTS()) -
// safe to read/write freely here without touching the real
// ~/.config/familiar/settings.json.
//
// That storage is still shared across every TEST() in this binary,
// though, so each test below establishes its own starting state with
// remove()/setValue() rather than assuming a clean slate, and cleans up
// after itself - same reasoning as CommandlineArgsTest's singleton note.

TEST(FamSettingsTest, ValueOrDefaultFallsBackWhenUnset)
{
    FamSettings settings;
    settings.remove(QStringLiteral("Items/arrange_gap"));
    EXPECT_EQ(settings.valueOrDefault(QStringLiteral("Items/arrange_gap")).toInt(),
             0);
}

TEST(FamSettingsTest, SetValueRoundTrips)
{
    FamSettings settings;
    settings.setValue(QStringLiteral("Items/arrange_gap"), 50);
    EXPECT_EQ(settings.valueOrDefault(QStringLiteral("Items/arrange_gap")).toInt(),
             50);
    settings.remove(QStringLiteral("Items/arrange_gap"));
}

TEST(FamSettingsTest, OutOfRangeValueFallsBackToDefaultOnRead)
{
    // setValue() itself doesn't validate - only valueOrDefault()'s read
    // path does (see FamSettings::setValue()/valueOrDefault() in
    // core/settings.cpp). A value written directly via setValue() that
    // fails Items/arrange_gap's own [0, 200] validator is still stored
    // as-is, but reading it back through valueOrDefault() silently
    // yields the field's default instead of the out-of-range value.
    FamSettings settings;
    settings.setValue(QStringLiteral("Items/arrange_gap"), 9999);
    EXPECT_EQ(settings.valueOrDefault(QStringLiteral("Items/arrange_gap")).toInt(),
             0);
    settings.remove(QStringLiteral("Items/arrange_gap"));
}

TEST(FamSettingsTest, ValueChangedTracksDefaultDrift)
{
    FamSettings settings;
    settings.remove(QStringLiteral("Items/arrange_gap"));
    EXPECT_FALSE(settings.valueChanged(QStringLiteral("Items/arrange_gap")));
    settings.setValue(QStringLiteral("Items/arrange_gap"), 50);
    EXPECT_TRUE(settings.valueChanged(QStringLiteral("Items/arrange_gap")));
    settings.remove(QStringLiteral("Items/arrange_gap"));
}

TEST(FamSettingsTest, RestoreDefaultsClearsItemsAndSaveGroups)
{
    FamSettings settings;
    settings.setValue(QStringLiteral("Items/arrange_gap"), 50);
    settings.setValue(QStringLiteral("Save/autosave_interval_seconds"), 30);

    settings.restoreDefaults();

    EXPECT_EQ(settings.valueOrDefault(QStringLiteral("Items/arrange_gap")).toInt(),
             0);
    EXPECT_EQ(settings
                 .valueOrDefault(QStringLiteral("Save/autosave_interval_seconds"))
                 .toInt(),
             5);
}

TEST(FamSettingsTest, UpdateRecentFilesDedupsPrependsAndCaps)
{
    SettingsHandler::getInstance()->setRecentFilesRaw({});
    FamSettings settings;

    // Already-absolute paths - updateRecentFiles() runs them through
    // QFileInfo::absoluteFilePath(), which is a no-op for these, so the
    // list this test reads back matches what it wrote verbatim instead
    // of depending on the current working directory.
    for (int i = 0; i < 12; ++i)
        settings.updateRecentFiles(QStringLiteral("/tmp/file_%1.fml").arg(i));

    // Re-adding an already-present file moves it to the front instead of
    // appearing twice.
    settings.updateRecentFiles(QStringLiteral("/tmp/file_5.fml"));

    const QStringList files = settings.getRecentFiles();
    EXPECT_EQ(files.size(), 10);
    EXPECT_EQ(files.first(), QStringLiteral("/tmp/file_5.fml"));
    EXPECT_EQ(files.count(QStringLiteral("/tmp/file_5.fml")), 1);

    SettingsHandler::getInstance()->setRecentFilesRaw({});
}
