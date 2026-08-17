#include "widgets/setting_row.h"

#include "core/settings.h"

#include <gtest/gtest.h>

#include <QCheckBox>
#include <QComboBox>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTest>

// Real end-to-end coverage of the row<->FamSettings wiring - the input
// control found via findChild<T*>() (no public getter exposes it, same
// as real code never needs one either: SettingRowBase's own contract is
// "construct it, it reflects storage; interact with it, storage
// updates"). show()+qWaitForWindowExposed() before sending real click/
// key events - see tests/widgets/flat_checkbox_test.cpp's own comment
// for why (QAbstractButton/QAbstractSpinBox click/key handling silently
// no-ops without a real exposed window).
//
// Each test establishes its own starting value and restores the default
// afterward (FamSettings::remove()) - SettingsHandler's storage is
// shared across every TEST() in this binary (redirected to a temp file
// by tests/support/settings_test_environment.h, not the real app
// config), same reasoning as FamSettingsTest's own singleton note.

TEST(AutosaveEnabledRowTest, ConstructedStateReflectsStoredValue)
{
    FamSettings settings;
    settings.setValue(QStringLiteral("Save/autosave_enabled"), true);

    AutosaveEnabledRow row;
    QCheckBox* checkbox = row.findChild<QCheckBox*>();
    ASSERT_NE(checkbox, nullptr);
    EXPECT_TRUE(checkbox->isChecked());

    settings.remove(QStringLiteral("Save/autosave_enabled"));
}

TEST(AutosaveEnabledRowTest, TogglingCheckboxPersistsToSettingsAndEmitsToggled)
{
    FamSettings settings;
    settings.remove(QStringLiteral("Save/autosave_enabled")); // default: false

    AutosaveEnabledRow row;
    row.resize(200, 30);
    row.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&row));

    QCheckBox* checkbox = row.findChild<QCheckBox*>();
    ASSERT_NE(checkbox, nullptr);
    EXPECT_FALSE(checkbox->isChecked());

    QSignalSpy toggledSpy(&row, &CheckboxSettingRow::toggled);

    QTest::mouseClick(checkbox, Qt::LeftButton);

    EXPECT_TRUE(checkbox->isChecked());
    EXPECT_TRUE(
        settings.valueOrDefault(QStringLiteral("Save/autosave_enabled"))
            .toBool());
    ASSERT_EQ(toggledSpy.count(), 1);
    EXPECT_TRUE(toggledSpy.takeFirst().at(0).toBool());

    settings.remove(QStringLiteral("Save/autosave_enabled"));
}

TEST(AutosaveEnabledRowTest, CtrlDoubleClickOnRowResetsToDefault)
{
    FamSettings settings;
    settings.setValue(QStringLiteral("Save/autosave_enabled"), true); // non-default

    AutosaveEnabledRow row;
    row.resize(300, 30); // label + stretch + checkbox - center lands on the
                        // stretch, not the checkbox, per SettingRowBase::
                        // mouseDoubleClickEvent()'s own "not already
                        // consumed by a child widget" comment
    row.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&row));

    QCheckBox* checkbox = row.findChild<QCheckBox*>();
    ASSERT_NE(checkbox, nullptr);
    ASSERT_TRUE(checkbox->isChecked());

    QTest::mouseDClick(&row, Qt::LeftButton, Qt::ControlModifier);

    EXPECT_FALSE(checkbox->isChecked());
    EXPECT_FALSE(
        settings.valueOrDefault(QStringLiteral("Save/autosave_enabled"))
            .toBool());
}

TEST(UndoHistorySizeRowTest, ConstructedFromStoredValueAndPersistsChanges)
{
    FamSettings settings;
    settings.setValue(QStringLiteral("Items/undo_history_size"), 250);

    UndoHistorySizeRow row;
    row.resize(300, 30);
    row.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&row));

    QSpinBox* spin = row.findChild<QSpinBox*>();
    ASSERT_NE(spin, nullptr);
    EXPECT_EQ(spin->value(), 250);

    QTest::keyClick(spin, Qt::Key_Up);

    EXPECT_EQ(spin->value(), 251);
    EXPECT_EQ(
        settings.valueOrDefault(QStringLiteral("Items/undo_history_size"))
            .toInt(),
        251);

    settings.remove(QStringLiteral("Items/undo_history_size"));
}

TEST(IntegerSettingRowTest, SetControlEnabledDisablesOnlyTheInputNotTheRow)
{
    UndoHistorySizeRow row;
    QSpinBox* spin = row.findChild<QSpinBox*>();
    ASSERT_NE(spin, nullptr);

    row.setControlEnabled(false);
    EXPECT_FALSE(spin->isEnabled());
    // The row itself (and its label) deliberately stay enabled - see
    // SettingRowBase::setControlEnabled()'s own comment: a fully-
    // disabled row loses its hover cursor along with the input.
    EXPECT_TRUE(row.isEnabled());

    row.setControlEnabled(true);
    EXPECT_TRUE(spin->isEnabled());
}

TEST(AutoOptimizeImportedImagesRowTest, SelectingOptionPersistsItsValueString)
{
    FamSettings settings;
    settings.remove(
        QStringLiteral("Items/auto_optimize_imported_images")); // default: "warn"

    AutoOptimizeImportedImagesRow row;
    QComboBox* combo = row.findChild<QComboBox*>();
    ASSERT_NE(combo, nullptr);
    EXPECT_EQ(combo->currentIndex(), 1); // off=0, warn=1, optimize_large=2

    combo->setCurrentIndex(2);

    EXPECT_EQ(settings
                 .valueOrDefault(
                     QStringLiteral("Items/auto_optimize_imported_images"))
                 .toString(),
             QStringLiteral("optimize_large"));

    settings.remove(QStringLiteral("Items/auto_optimize_imported_images"));
}
