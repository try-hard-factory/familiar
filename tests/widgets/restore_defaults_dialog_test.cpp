#include "widgets/restore_defaults_dialog.h"

#include <gtest/gtest.h>

#include <QCheckBox>
#include <QPushButton>

using familiar::SettingsCategory;

namespace {

QCheckBox* checkbox(RestoreDefaultsDialog& dialog, const QString& label)
{
    for (auto* box : dialog.findChildren<QCheckBox*>()) {
        if (box->text() == label) {
            return box;
        }
    }
    return nullptr;
}

QPushButton* button(RestoreDefaultsDialog& dialog, const QString& label)
{
    for (auto* candidate : dialog.findChildren<QPushButton*>()) {
        if (candidate->text() == label) {
            return candidate;
        }
    }
    return nullptr;
}

} // namespace

TEST(RestoreDefaultsDialogTest, AllSelectsAndClearsEveryCategory)
{
    RestoreDefaultsDialog dialog;
    auto* all = checkbox(dialog, QStringLiteral("All"));
    ASSERT_NE(all, nullptr);

    all->setChecked(true);
    EXPECT_EQ(dialog.checked_categories().size(), 4);

    all->setChecked(false);
    EXPECT_TRUE(dialog.checked_categories().isEmpty());
}

TEST(RestoreDefaultsDialogTest, IndividualSelectionKeepsAllInSync)
{
    RestoreDefaultsDialog dialog;
    auto* all = checkbox(dialog, QStringLiteral("All"));
    auto* performance = checkbox(dialog, QStringLiteral("Performance"));
    auto* images = checkbox(dialog, QStringLiteral("Images & Items"));
    auto* colors = checkbox(dialog, QStringLiteral("Colors"));
    auto* shortcuts = checkbox(dialog, QStringLiteral("Keyboard Shortcuts"));
    ASSERT_NE(all, nullptr);
    ASSERT_NE(performance, nullptr);
    ASSERT_NE(images, nullptr);
    ASSERT_NE(colors, nullptr);
    ASSERT_NE(shortcuts, nullptr);

    performance->setChecked(true);
    images->setChecked(true);
    colors->setChecked(true);
    shortcuts->setChecked(true);
    EXPECT_TRUE(all->isChecked());
    EXPECT_EQ(dialog.checked_categories().size(), 4);

    images->setChecked(false);
    EXPECT_FALSE(all->isChecked());
    EXPECT_FALSE(dialog.checked_categories().contains(SettingsCategory::ImagesAndItems));
    EXPECT_TRUE(dialog.checked_categories().contains(SettingsCategory::Performance));
}

TEST(RestoreDefaultsDialogTest, CancelAndRestoreGiveDistinctResults)
{
    RestoreDefaultsDialog cancelDialog;
    auto* cancel = button(cancelDialog, QStringLiteral("Cancel"));
    ASSERT_NE(cancel, nullptr);
    cancel->click();
    EXPECT_EQ(cancelDialog.result(), QDialog::Rejected);

    RestoreDefaultsDialog restoreDialog;
    auto* restore = button(restoreDialog, QStringLiteral("Restore to Default"));
    ASSERT_NE(restore, nullptr);
    restore->click();
    EXPECT_EQ(restoreDialog.result(), QDialog::Accepted);
}

TEST(RestoreDefaultsDialogTest, CategoryKeysMatchTheSettingsTheyCanReset)
{
    const auto performance = RestoreDefaultsDialog::fam_settings_keys_for(
        SettingsCategory::Performance);
    const auto images = RestoreDefaultsDialog::fam_settings_keys_for(
        SettingsCategory::ImagesAndItems);

    EXPECT_TRUE(performance.contains(QStringLiteral("Save/autosave_enabled")));
    EXPECT_TRUE(performance.contains(QStringLiteral("Items/undo_history_size")));
    EXPECT_TRUE(images.contains(QStringLiteral("Items/image_allocation_limit")));
    EXPECT_FALSE(images.contains(QStringLiteral("Items/undo_history_size")));
    EXPECT_TRUE(RestoreDefaultsDialog::fam_settings_keys_for(
                    SettingsCategory::Colors).isEmpty());
}
