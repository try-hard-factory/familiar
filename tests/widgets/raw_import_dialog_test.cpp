#include "widgets/raw_import_dialog.h"

#include <gtest/gtest.h>

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>

namespace {

QPushButton* button(RawImportDialog& dialog, const QString& text)
{
    for (auto* candidate : dialog.findChildren<QPushButton*>()) {
        if (candidate->text() == text) {
            return candidate;
        }
    }
    return nullptr;
}

} // namespace

TEST(RawImportDialogTest, OptimizeAcceptsWithDefaultFlagsOff)
{
    RawImportDialog dialog(nullptr, QStringLiteral("camera.NEF"));
    auto* optimize = button(dialog, QStringLiteral("Optimize image"));
    ASSERT_NE(optimize, nullptr);
    EXPECT_FALSE(dialog.apply_to_queue());
    EXPECT_FALSE(dialog.remember_choice());

    optimize->click();

    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(dialog.choice(), RawImportChoice::Optimize);
}

TEST(RawImportDialogTest, KeepOriginalPreservesQueueAndRememberFlags)
{
    RawImportDialog dialog(nullptr, QStringLiteral("camera.CR3"));
    const auto checkboxes = dialog.findChildren<QCheckBox*>();
    auto* keep = button(dialog, QStringLiteral("Keep original"));
    ASSERT_EQ(checkboxes.size(), 2);
    ASSERT_NE(keep, nullptr);

    checkboxes[0]->setChecked(true);
    checkboxes[1]->setChecked(true);
    keep->click();

    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(dialog.choice(), RawImportChoice::KeepOriginal);
    EXPECT_TRUE(dialog.apply_to_queue());
    EXPECT_TRUE(dialog.remember_choice());
}

TEST(RawImportDialogTest, CancelRejectsWithoutChoosing)
{
    RawImportDialog dialog(nullptr, QStringLiteral("camera.NEF"));
    auto* close = dialog.findChild<QPushButton*>(QStringLiteral("ridCloseBtn"));
    ASSERT_NE(close, nullptr);

    close->click();

    EXPECT_EQ(dialog.result(), QDialog::Rejected);
}

TEST(RawImportDialogTest, FilenameIsEscapedAndSourceFormatIsVisible)
{
    RawImportDialog dialog(nullptr, QStringLiteral("/tmp/<portrait>.NEF"));
    bool escapedName = false;
    bool sourceFormat = false;
    for (auto* label : dialog.findChildren<QLabel*>()) {
        escapedName |= label->text().contains(QStringLiteral("&lt;portrait&gt;.NEF"));
        sourceFormat |= label->text().contains(QStringLiteral("Format: NEF"));
    }

    EXPECT_TRUE(escapedName);
    EXPECT_TRUE(sourceFormat);
}
