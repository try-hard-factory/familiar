#include "mainwindow.h"
#include "widgets/help_dialog.h"

#include <gtest/gtest.h>

#include <QLabel>

TEST(HelpDialogTest, ShowsSupportLinksAndShortcutEntryPoint)
{
    MainWindow window;
    HelpDialog dialog(&window);
    bool discord = false;
    bool issues = false;
    bool shortcutSettings = false;
    for (auto* label : dialog.findChildren<QLabel*>()) {
        discord |= label->text().contains(
            QStringLiteral("https://discord.gg/3672sNT5qA"));
        issues |= label->text().contains(
            QStringLiteral("https://github.com/try-hard-factory/familiar/issues"));
        shortcutSettings |= label->text().contains(
            QStringLiteral("href=\"#\""))
                            && label->text().contains(
                                QStringLiteral("Keyboard Shortcuts"));
    }
    EXPECT_TRUE(discord);
    EXPECT_TRUE(issues);
    EXPECT_TRUE(shortcutSettings);
}
