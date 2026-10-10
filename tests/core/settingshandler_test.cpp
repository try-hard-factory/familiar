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
    auto* h = SettingsHandler::get_instance();
    h->set_current_preset(EPresets::kCustom2);
    EXPECT_EQ(h->current_preset(), int(EPresets::kCustom2));
    h->remove(QStringLiteral("current_preset"));
}

TEST(SettingsHandlerTest, MasterOpacityGetSetRoundTrips)
{
    auto* h = SettingsHandler::get_instance();
    const OL opacities{{kDarkPreset, 100},
                       {kLightPreset, 150},
                       {kCustom1, 200},
                       {kCustom2, 255},
                       {kCustom3, 10},
                       {kCustom4, 0}};
    h->set_master_opacity(opacities);
    EXPECT_EQ(h->master_opacity(), opacities);
    h->remove(QStringLiteral("master_opacity"));
}

TEST(SettingsHandlerTest, ColorPresetGetSetRoundTrips)
{
    auto* h = SettingsHandler::get_instance();
    const CL preset = makeColorList(42);

    h->set_dark_color_preset(preset);
    EXPECT_EQ(h->dark_color_preset(), preset);

    h->remove(QStringLiteral("dark_color_preset"));
}

TEST(SettingsHandlerTest, GetCurrentColorPresetDispatchesOnCurrentPreset)
{
    auto* h = SettingsHandler::get_instance();
    const CL lightPreset = makeColorList(11);
    const CL custom1Preset = makeColorList(22);

    h->set_current_preset(EPresets::kLightPreset);
    h->set_light_color_preset(lightPreset);
    EXPECT_EQ(h->get_current_color_preset(), lightPreset);

    h->set_current_preset(EPresets::kCustom1);
    h->set_custom_preset1(custom1Preset);
    EXPECT_EQ(h->get_current_color_preset(), custom1Preset);

    h->remove(QStringLiteral("current_preset"));
    h->remove(QStringLiteral("light_color_preset"));
    h->remove(QStringLiteral("custom_preset1"));
}

TEST(SettingsHandlerTest, SetCurrentColorPresetDispatchesOnCurrentPreset)
{
    auto* h = SettingsHandler::get_instance();
    const CL preset = makeColorList(77);

    h->set_current_preset(EPresets::kCustom3);
    h->set_current_color_preset(preset);
    EXPECT_EQ(h->custom_preset3(), preset);
    // Only the dispatched-to preset changed - a sibling stays untouched.
    EXPECT_NE(h->custom_preset4(), preset);

    h->remove(QStringLiteral("current_preset"));
    h->remove(QStringLiteral("custom_preset3"));
}

TEST(SettingsHandlerTest, GetSetCurrentOpacityTargetsTheActivePresetSlot)
{
    auto* h = SettingsHandler::get_instance();
    h->set_current_preset(EPresets::kDarkPreset);

    h->set_current_opacity(128);
    EXPECT_EQ(h->get_current_opacity(), 128);
    EXPECT_EQ(h->master_opacity()[EPresets::kDarkPreset], 128);

    h->remove(QStringLiteral("current_preset"));
    h->remove(QStringLiteral("master_opacity"));
}

TEST(SettingsHandlerTest, SetDefaultCurrentPresetResetsColorsAndOpacity)
{
    auto* h = SettingsHandler::get_instance();
    h->set_current_preset(EPresets::kDarkPreset);
    const CL custom = makeColorList(99);
    h->set_dark_color_preset(custom);
    h->set_current_opacity(50);

    h->set_default_current_preset();

    // setDefaultCurrentPreset() removes the JSON key entirely (falls
    // back to the built-in default), so it no longer reads back as the
    // custom value that was just written.
    EXPECT_NE(h->dark_color_preset(), custom);
    EXPECT_EQ(h->get_current_opacity(), 255);

    h->remove(QStringLiteral("current_preset"));
}

TEST(SettingsHandlerTest, JsonValueSetRemoveRoundTrip)
{
    auto* h = SettingsHandler::get_instance();
    h->remove_json_group(QStringLiteral("TestGroup"));

    EXPECT_TRUE(h->json_value(QStringLiteral("TestGroup"), QStringLiteral("key"))
                    .isUndefined());

    h->set_json_value(QStringLiteral("TestGroup"),
                      QStringLiteral("key"),
                      QJsonValue(QStringLiteral("value")));
    EXPECT_EQ(h->json_value(QStringLiteral("TestGroup"), QStringLiteral("key"))
                  .toString(),
              QStringLiteral("value"));

    h->remove_json_value(QStringLiteral("TestGroup"), QStringLiteral("key"));
    EXPECT_TRUE(h->json_value(QStringLiteral("TestGroup"), QStringLiteral("key"))
                    .isUndefined());

    h->remove_json_group(QStringLiteral("TestGroup"));
}

TEST(SettingsHandlerTest, RecentFilesRawRoundTrips)
{
    auto* h = SettingsHandler::get_instance();
    const QStringList files{QStringLiteral("/a.fml"), QStringLiteral("/b.fml")};
    h->set_recent_files_raw(files);
    EXPECT_EQ(h->recent_files_raw(), files);
    h->set_recent_files_raw({});
}

TEST(SettingsHandlerTest, ExportThenImportRestoresJsonValue)
{
    auto* h = SettingsHandler::get_instance();
    h->set_json_value(QStringLiteral("TestGroup"),
                      QStringLiteral("key"),
                      QJsonValue(123));

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString exportPath = dir.filePath(QStringLiteral("exported.json"));
    ASSERT_TRUE(h->export_settings_to(exportPath));

    // Mutate the live document after exporting - import should overwrite
    // it back to what was on disk, not merge with the current state.
    h->set_json_value(QStringLiteral("TestGroup"),
                      QStringLiteral("key"),
                      QJsonValue(456));

    ASSERT_TRUE(h->import_settings_from(exportPath));
    EXPECT_EQ(h->json_value(QStringLiteral("TestGroup"), QStringLiteral("key"))
                  .toInt(),
              123);

    h->remove_json_group(QStringLiteral("TestGroup"));
}

TEST(SettingsHandlerTest, ImportFromMissingFileFails)
{
    auto* h = SettingsHandler::get_instance();
    EXPECT_FALSE(h->import_settings_from(
        QStringLiteral("/nonexistent/path/does-not-exist.json")));
}

TEST(SettingsHandlerTest, ExportToUnwritablePathFails)
{
    auto* h = SettingsHandler::get_instance();
    EXPECT_FALSE(
        h->export_settings_to(QStringLiteral("/nonexistent-dir-xyz/out.json")));
}

TEST(SettingsHandlerTest, ValueRemoveResetRoundTrip)
{
    auto* h = SettingsHandler::get_instance();
    h->remove(QStringLiteral("option0"));

    // Unset -> Bool's fallback (true, see recognizedGeneralOptions in
    // core/settingshandler.cpp).
    EXPECT_TRUE(h->value(QStringLiteral("option0")).toBool());

    h->set_value(QStringLiteral("option0"), false);
    EXPECT_FALSE(h->value(QStringLiteral("option0")).toBool());

    h->reset_value(QStringLiteral("option0"));
    EXPECT_TRUE(h->value(QStringLiteral("option0")).toBool());

    h->remove(QStringLiteral("option0"));
}
