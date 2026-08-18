#include "core/settingshandler.h"

#include <gtest/gtest.h>

#include <QTemporaryDir>

// SettingsHandler owns the single settings.json document (see its own
// header comment) - tests/support/settings_test_environment.h has
// already redirected its singleton storage to a throwaway temp file, so
// it's safe to read/write here without touching the real
// ~/.config/familiar/settings.json. That storage is still shared across
// every TEST() in this binary though, so each test below establishes its
// own starting state and cleans up after itself - same reasoning as
// FamSettingsTest (tests/core/famsettings_test.cpp).
//
// This file covers SettingsHandler's OWN logic - the "Colors" JSON group
// (currentPreset/masterOpacity/darkColorPreset/.../getCurrentColorPreset
// dispatch) plus the raw jsonValue()/setJsonValue()/export/import layer.
// FamSettings/KeyboardSettings-backed facade methods are already covered
// by famsettings_test.cpp/keyboard_settings_test.cpp respectively.

using CL = QMap<int, QColor>;
using OL = QMap<int, int>;

namespace {
CL makeColorList(int seed)
{
    return CL{{kBackgroundColor, QColor(seed, 0, 0)},
             {kCanvasColor, QColor(0, seed, 0)},
             {kBorderColor, QColor(0, 0, seed)},
             {kTextColor, QColor(seed, seed, 0)},
             {kSelectionColor, QColor(seed, 0, seed)}};
}
} // namespace

TEST(SettingsHandlerTest, CurrentPresetGetSetRoundTrips)
{
    auto* h = SettingsHandler::getInstance();
    h->setCurrentPreset(EPresets::kCustom2);
    EXPECT_EQ(h->currentPreset(), int(EPresets::kCustom2));
    h->remove(QStringLiteral("currentPreset"));
}

TEST(SettingsHandlerTest, MasterOpacityGetSetRoundTrips)
{
    auto* h = SettingsHandler::getInstance();
    const OL opacities{{kDarkPreset, 100},
                       {kLightPreset, 150},
                       {kCustom1, 200},
                       {kCustom2, 255},
                       {kCustom3, 10},
                       {kCustom4, 0}};
    h->setMasterOpacity(opacities);
    EXPECT_EQ(h->masterOpacity(), opacities);
    h->remove(QStringLiteral("masterOpacity"));
}

TEST(SettingsHandlerTest, ColorPresetGetSetRoundTrips)
{
    auto* h = SettingsHandler::getInstance();
    const CL preset = makeColorList(42);

    h->setDarkColorPreset(preset);
    EXPECT_EQ(h->darkColorPreset(), preset);

    h->remove(QStringLiteral("darkColorPreset"));
}

TEST(SettingsHandlerTest, GetCurrentColorPresetDispatchesOnCurrentPreset)
{
    auto* h = SettingsHandler::getInstance();
    const CL lightPreset = makeColorList(11);
    const CL custom1Preset = makeColorList(22);

    h->setCurrentPreset(EPresets::kLightPreset);
    h->setLightColorPreset(lightPreset);
    EXPECT_EQ(h->getCurrentColorPreset(), lightPreset);

    h->setCurrentPreset(EPresets::kCustom1);
    h->setCustomPreset1(custom1Preset);
    EXPECT_EQ(h->getCurrentColorPreset(), custom1Preset);

    h->remove(QStringLiteral("currentPreset"));
    h->remove(QStringLiteral("lightColorPreset"));
    h->remove(QStringLiteral("customPreset1"));
}

TEST(SettingsHandlerTest, SetCurrentColorPresetDispatchesOnCurrentPreset)
{
    auto* h = SettingsHandler::getInstance();
    const CL preset = makeColorList(77);

    h->setCurrentPreset(EPresets::kCustom3);
    h->setCurrentColorPreset(preset);
    EXPECT_EQ(h->customPreset3(), preset);
    // Only the dispatched-to preset changed - a sibling stays untouched.
    EXPECT_NE(h->customPreset4(), preset);

    h->remove(QStringLiteral("currentPreset"));
    h->remove(QStringLiteral("customPreset3"));
}

TEST(SettingsHandlerTest, GetSetCurrentOpacityTargetsTheActivePresetSlot)
{
    auto* h = SettingsHandler::getInstance();
    h->setCurrentPreset(EPresets::kDarkPreset);

    h->setCurrentOpacity(128);
    EXPECT_EQ(h->getCurrentOpacity(), 128);
    EXPECT_EQ(h->masterOpacity()[EPresets::kDarkPreset], 128);

    h->remove(QStringLiteral("currentPreset"));
    h->remove(QStringLiteral("masterOpacity"));
}

TEST(SettingsHandlerTest, SetDefaultCurrentPresetResetsColorsAndOpacity)
{
    auto* h = SettingsHandler::getInstance();
    h->setCurrentPreset(EPresets::kDarkPreset);
    const CL custom = makeColorList(99);
    h->setDarkColorPreset(custom);
    h->setCurrentOpacity(50);

    h->setDefaultCurrentPreset();

    // setDefaultCurrentPreset() removes the JSON key entirely (falls
    // back to the built-in default), so it no longer reads back as the
    // custom value that was just written.
    EXPECT_NE(h->darkColorPreset(), custom);
    EXPECT_EQ(h->getCurrentOpacity(), 255);

    h->remove(QStringLiteral("currentPreset"));
}

TEST(SettingsHandlerTest, JsonValueSetRemoveRoundTrip)
{
    auto* h = SettingsHandler::getInstance();
    h->removeJsonGroup(QStringLiteral("TestGroup"));

    EXPECT_TRUE(
        h->jsonValue(QStringLiteral("TestGroup"), QStringLiteral("key"))
            .isUndefined());

    h->setJsonValue(QStringLiteral("TestGroup"),
                    QStringLiteral("key"),
                    QJsonValue(QStringLiteral("value")));
    EXPECT_EQ(h->jsonValue(QStringLiteral("TestGroup"), QStringLiteral("key"))
                  .toString(),
              QStringLiteral("value"));

    h->removeJsonValue(QStringLiteral("TestGroup"), QStringLiteral("key"));
    EXPECT_TRUE(
        h->jsonValue(QStringLiteral("TestGroup"), QStringLiteral("key"))
            .isUndefined());

    h->removeJsonGroup(QStringLiteral("TestGroup"));
}

TEST(SettingsHandlerTest, RecentFilesRawRoundTrips)
{
    auto* h = SettingsHandler::getInstance();
    const QStringList files{QStringLiteral("/a.fml"), QStringLiteral("/b.fml")};
    h->setRecentFilesRaw(files);
    EXPECT_EQ(h->recentFilesRaw(), files);
    h->setRecentFilesRaw({});
}

TEST(SettingsHandlerTest, ExportThenImportRestoresJsonValue)
{
    auto* h = SettingsHandler::getInstance();
    h->setJsonValue(QStringLiteral("TestGroup"),
                    QStringLiteral("key"),
                    QJsonValue(123));

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString exportPath = dir.filePath(QStringLiteral("exported.json"));
    ASSERT_TRUE(h->exportSettingsTo(exportPath));

    // Mutate the live document after exporting - import should overwrite
    // it back to what was on disk, not merge with the current state.
    h->setJsonValue(QStringLiteral("TestGroup"),
                    QStringLiteral("key"),
                    QJsonValue(456));

    ASSERT_TRUE(h->importSettingsFrom(exportPath));
    EXPECT_EQ(
        h->jsonValue(QStringLiteral("TestGroup"), QStringLiteral("key")).toInt(),
        123);

    h->removeJsonGroup(QStringLiteral("TestGroup"));
}

TEST(SettingsHandlerTest, ImportFromMissingFileFails)
{
    auto* h = SettingsHandler::getInstance();
    EXPECT_FALSE(h->importSettingsFrom(
        QStringLiteral("/nonexistent/path/does-not-exist.json")));
}

TEST(SettingsHandlerTest, ExportToUnwritablePathFails)
{
    auto* h = SettingsHandler::getInstance();
    EXPECT_FALSE(h->exportSettingsTo(
        QStringLiteral("/nonexistent-dir-xyz/out.json")));
}

TEST(SettingsHandlerTest, ValueRemoveResetRoundTrip)
{
    auto* h = SettingsHandler::getInstance();
    h->remove(QStringLiteral("option0"));

    // Unset -> Bool's fallback (true, see recognizedGeneralOptions in
    // core/settingshandler.cpp).
    EXPECT_TRUE(h->value(QStringLiteral("option0")).toBool());

    h->setValue(QStringLiteral("option0"), false);
    EXPECT_FALSE(h->value(QStringLiteral("option0")).toBool());

    h->resetValue(QStringLiteral("option0"));
    EXPECT_TRUE(h->value(QStringLiteral("option0")).toBool());

    h->remove(QStringLiteral("option0"));
}
