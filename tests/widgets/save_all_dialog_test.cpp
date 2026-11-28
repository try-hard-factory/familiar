#include "mainwindow.h"
#include "widgets/save_all_dialog.h"

#include <gtest/gtest.h>

#include <QCheckBox>
#include <QPushButton>

TEST(SaveAllDialogTest, ListsEveryUnsavedDocumentAndCanBeCancelled)
{
    MainWindow window;
    SaveAllDialog dialog(&window,
                         {{1, QStringLiteral("/tmp/board.fml")}, {2, QString()}});
    // This production dialog deletes itself on close. The stack-owned test
    // instance uses ordinary QDialog lifetime so Cancel can be exercised.
    dialog.setAttribute(Qt::WA_DeleteOnClose, false);

    const auto choices = dialog.findChildren<QCheckBox*>();
    ASSERT_EQ(choices.size(), 2);
    EXPECT_TRUE(choices[0]->isChecked());
    EXPECT_TRUE(choices[1]->isChecked());
    EXPECT_EQ(choices[0]->text(), QStringLiteral("/tmp/board.fml"));
    EXPECT_EQ(choices[1]->text(), QStringLiteral("Untitled"));

    QPushButton* cancel = nullptr;
    for (auto* button : dialog.findChildren<QPushButton*>()) {
        if (button->text() == QStringLiteral("Cancel")) {
            cancel = button;
            break;
        }
    }
    ASSERT_NE(cancel, nullptr);
    cancel->click();
    EXPECT_FALSE(dialog.isVisible());
}
