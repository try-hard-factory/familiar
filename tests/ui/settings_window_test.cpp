#include "ui/settings_window.h"
#include "core/settings.h"
#include "widgets/restore_defaults_dialog.h"
#include "widgets/setting_row.h"

#include <gtest/gtest.h>

#include <QButtonGroup>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QScopeGuard>
#include <QStackedWidget>
#include <QTimer>

namespace {

QLineEdit* search_box(SettingsWindow& window)
{
    for (auto* edit : window.findChildren<QLineEdit*>()) {
        if (edit->placeholderText() == QStringLiteral("Search")) {
            return edit;
        }
    }
    return nullptr;
}

} // namespace

TEST(SettingsWindowTest, CategorySelectionSwitchesToTheRequestedPage)
{
    SettingsWindow window(nullptr);
    auto* stack = window.findChild<QStackedWidget*>();
    auto* buttons = window.findChild<QButtonGroup*>();
    ASSERT_NE(stack, nullptr);
    ASSERT_NE(buttons, nullptr);
    ASSERT_EQ(stack->count(), 4);
    EXPECT_EQ(stack->currentIndex(), 0);

    window.select_category(QStringLiteral("Keyboard Shortcuts"));
    EXPECT_EQ(stack->currentIndex(), 3);
    EXPECT_TRUE(buttons->button(3)->isChecked());

    window.select_category(QStringLiteral("Unknown category"));
    EXPECT_EQ(stack->currentIndex(), 3);

    buttons->button(1)->click();
    EXPECT_EQ(stack->currentIndex(), 1);
}

TEST(SettingsWindowTest, SearchFindsRowsAndMovesToAVisibleCategory)
{
    SettingsWindow window(nullptr);
    auto* search = search_box(window);
    auto* stack = window.findChild<QStackedWidget*>();
    auto* buttons = window.findChild<QButtonGroup*>();
    ASSERT_NE(search, nullptr);
    ASSERT_NE(stack, nullptr);
    ASSERT_NE(buttons, nullptr);

    window.select_category(QStringLiteral("Colors"));
    search->setText(QStringLiteral("Autosave"));
    EXPECT_EQ(stack->currentIndex(), 0);
    EXPECT_FALSE(buttons->button(0)->isHidden());
    EXPECT_TRUE(buttons->button(1)->isHidden());
    EXPECT_TRUE(buttons->button(2)->isHidden());
    EXPECT_TRUE(buttons->button(3)->isHidden());

    bool matchedRowVisible = false;
    bool unrelatedRowHidden = false;
    for (auto* row : stack->widget(0)->findChildren<SettingRowBase*>()) {
        if (row->objectName().contains(QStringLiteral("Autosave"),
                                       Qt::CaseInsensitive)) {
            matchedRowVisible |= !row->isHidden();
        } else {
            unrelatedRowHidden |= row->isHidden();
        }
    }
    EXPECT_TRUE(matchedRowVisible);
    EXPECT_TRUE(unrelatedRowHidden);

    search->clear();
    for (auto* button : buttons->buttons()) {
        EXPECT_FALSE(button->isHidden());
    }
}

TEST(SettingsWindowTest, CategoryNameSearchLeavesItsWholePageAvailable)
{
    SettingsWindow window(nullptr);
    auto* search = search_box(window);
    auto* stack = window.findChild<QStackedWidget*>();
    auto* buttons = window.findChild<QButtonGroup*>();
    ASSERT_NE(search, nullptr);
    ASSERT_NE(stack, nullptr);
    ASSERT_NE(buttons, nullptr);

    search->setText(QStringLiteral("Images & Items"));
    EXPECT_EQ(stack->currentIndex(), 1);
    EXPECT_FALSE(buttons->button(1)->isHidden());
    EXPECT_TRUE(buttons->button(0)->isHidden());
    for (auto* row : stack->widget(1)->findChildren<SettingRowBase*>()) {
        EXPECT_FALSE(row->isHidden());
    }
}

TEST(SettingsWindowTest, RestoringPerformanceLeavesImageSettingsAlone)
{
    const QString autosave = QStringLiteral("Save/autosave_enabled");
    const QString gap = QStringLiteral("Items/arrange_gap");
    const QVariant oldAutosave = FamSettings::value_or_default(autosave);
    const QVariant oldGap = FamSettings::value_or_default(gap);
    const auto restore = qScopeGuard([&] {
        FamSettings::set_value(autosave, oldAutosave);
        FamSettings::set_value(gap, oldGap);
    });
    const QVariant defaultAutosave = FamSettings::fields()[autosave].defaultValue;
    const int defaultGap = FamSettings::fields()[gap].defaultValue.toInt();
    FamSettings::set_value(autosave, !defaultAutosave.toBool());
    FamSettings::set_value(gap, defaultGap + 7);

    SettingsWindow window(nullptr);
    QPushButton* reset = nullptr;
    for (auto* button : window.findChildren<QPushButton*>()) {
        if (button->text() == QStringLiteral("Restore Defaults")) {
            reset = button;
            break;
        }
    }
    ASSERT_NE(reset, nullptr);
    bool confirmed = false;
    QTimer::singleShot(0, &window, [&] {
        auto* dialog = window.findChild<RestoreDefaultsDialog*>();
        if (!dialog) {
            return;
        }
        QCheckBox* performance = nullptr;
        for (auto* box : dialog->findChildren<QCheckBox*>()) {
            if (box->text() == QStringLiteral("Performance")) {
                performance = box;
            }
            box->setChecked(false);
        }
        if (!performance) {
            dialog->reject();
            return;
        }
        performance->setChecked(true);
        dialog->accept();
        confirmed = true;
    });

    reset->click();

    EXPECT_TRUE(confirmed);
    EXPECT_EQ(FamSettings::value_or_default(autosave), defaultAutosave);
    EXPECT_EQ(FamSettings::value_or_default(gap).toInt(), defaultGap + 7);
}
