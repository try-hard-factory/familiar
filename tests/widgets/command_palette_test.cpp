#include "actions/actions.h"
#include "core/settingshandler.h"
#include "widgets/command_palette.h"

#include <gtest/gtest.h>

#include <QAction>
#include <QDialog>
#include <QJsonArray>
#include <QJsonValue>
#include <QKeySequence>
#include <QLineEdit>
#include <QTest>
#include <QTreeWidget>

#include <memory>
#include <vector>

namespace {

class ScopedPaletteHistory
{
public:
    ScopedPaletteHistory()
        : previous_(SettingsHandler::get_instance()
                        ->json_value(QStringLiteral("CommandPalette"),
                                     QStringLiteral("recent_actions")))
    {}

    ~ScopedPaletteHistory()
    {
        auto* settings = SettingsHandler::get_instance();
        if (previous_.isUndefined()) {
            settings->remove_json_value(QStringLiteral("CommandPalette"),
                                        QStringLiteral("recent_actions"));
        } else {
            settings->set_json_value(QStringLiteral("CommandPalette"),
                                     QStringLiteral("recent_actions"),
                                     previous_);
        }
    }

private:
    QJsonValue previous_;
};

} // namespace

TEST(CommandPaletteTest, SearchShowsCurrentShortcutAndSelectsAction)
{
    ScopedPaletteHistory history;
    const QString id = QStringLiteral("palette_test_open");
    SettingsHandler::get_instance()->remove_json_value(QStringLiteral(
                                                           "Actions"),
                                                       id);

    ActionRegistry registry;
    registry.add(Action::make(id,
                              QStringLiteral("Open Project"),
                              {},
                              {QStringLiteral("Ctrl+O")}));
    QAction openAction(QStringLiteral("Open Project"), nullptr);
    registry.find(id)->set_qaction(&openAction);
    registry.find(id)->set_shortcuts({QStringLiteral("Ctrl+Alt+O")});

    CommandPalette palette(registry);
    palette.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&palette));
    auto* search = palette.findChild<QLineEdit*>(
        QStringLiteral("commandPaletteSearch"));
    auto* results = palette.findChild<QTreeWidget*>(
        QStringLiteral("commandPaletteResults"));
    ASSERT_NE(search, nullptr);
    ASSERT_NE(results, nullptr);
    EXPECT_TRUE(search->hasFocus());

    search->setText(QStringLiteral("PROJECT"));
    ASSERT_EQ(results->topLevelItemCount(), 1);
    EXPECT_EQ(results->topLevelItem(0)->text(0), QStringLiteral("Open Project"));
    EXPECT_EQ(results->topLevelItem(0)->text(2),
              QKeySequence(QStringLiteral("Ctrl+Alt+O"))
                  .toString(QKeySequence::NativeText));

    QTest::keyClick(search, Qt::Key_Return);
    EXPECT_EQ(palette.result(), int(QDialog::Accepted));
    EXPECT_EQ(palette.selected_action(), &openAction);

    SettingsHandler::get_instance()->remove_json_value(QStringLiteral(
                                                           "Actions"),
                                                       id);
}

TEST(CommandPaletteTest, DisabledCommandExplainsWhyAndCannotRun)
{
    ActionRegistry registry;
    registry.add(Action::make(QStringLiteral("palette_test_crop"),
                              QStringLiteral("Crop"),
                              {},
                              {QStringLiteral("Shift+C")},
                              false,
                              false,
                              QStringLiteral("active_when_single_image")));
    QAction cropAction(QStringLiteral("Crop"), nullptr);
    cropAction.setEnabled(false);
    registry.find(QStringLiteral("palette_test_crop"))->set_qaction(&cropAction);

    CommandPalette palette(registry);
    palette.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&palette));
    auto* search = palette.findChild<QLineEdit*>(
        QStringLiteral("commandPaletteSearch"));
    auto* results = palette.findChild<QTreeWidget*>(
        QStringLiteral("commandPaletteResults"));
    ASSERT_NE(search, nullptr);
    ASSERT_NE(results, nullptr);

    search->setText(QStringLiteral("crop"));
    ASSERT_EQ(results->topLevelItemCount(), 1);
    EXPECT_TRUE(results->topLevelItem(0)->isDisabled());
    EXPECT_EQ(results->topLevelItem(0)->text(1),
              QStringLiteral("Select one image"));
    QTest::keyClick(search, Qt::Key_Return);
    EXPECT_EQ(palette.selected_action(), nullptr);
    EXPECT_TRUE(palette.isVisible());

    QTest::keyClick(search, Qt::Key_Escape);
    EXPECT_EQ(palette.result(), int(QDialog::Rejected));
}

TEST(CommandPaletteTest, ShowsFiveRecentCommandsAndPersistsNewSelection)
{
    ScopedPaletteHistory history;
    ActionRegistry registry;
    std::vector<std::unique_ptr<QAction>> qactions;
    QJsonArray saved;
    for (int index = 0; index < 6; ++index) {
        const QString id = QStringLiteral("palette_recent_%1").arg(index);
        const QString name = QStringLiteral("Command %1").arg(index);
        registry.add(Action::make(id, name));
        qactions.push_back(std::make_unique<QAction>(name));
        registry.find(id)->set_qaction(qactions.back().get());
        saved.append(id);
    }
    SettingsHandler::get_instance()
        ->set_json_value(QStringLiteral("CommandPalette"),
                         QStringLiteral("recent_actions"),
                         saved);

    CommandPalette palette(registry);
    palette.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&palette));
    auto* search = palette.findChild<QLineEdit*>(
        QStringLiteral("commandPaletteSearch"));
    auto* results = palette.findChild<QTreeWidget*>(
        QStringLiteral("commandPaletteResults"));
    ASSERT_NE(search, nullptr);
    ASSERT_NE(results, nullptr);
    ASSERT_EQ(results->topLevelItemCount(), 8);
    EXPECT_EQ(results->topLevelItem(0)->text(0),
              QStringLiteral("RECENTLY USED"));
    for (int index = 0; index < 5; ++index) {
        EXPECT_EQ(results->topLevelItem(index + 1)->text(0),
                  QStringLiteral("Command %1").arg(index));
    }
    EXPECT_EQ(results->topLevelItem(6)->text(0),
              QStringLiteral("OTHER COMMANDS"));
    EXPECT_EQ(results->topLevelItem(7)->text(0), QStringLiteral("Command 5"));

    search->setText(QStringLiteral("Command 5"));
    ASSERT_EQ(results->topLevelItemCount(), 1);
    QTest::keyClick(search, Qt::Key_Return);
    ASSERT_EQ(palette.selected_action(), qactions.back().get());
    const QJsonArray updated
        = SettingsHandler::get_instance()
              ->json_value(QStringLiteral("CommandPalette"),
                           QStringLiteral("recent_actions"))
              .toArray();
    ASSERT_EQ(updated.size(), 5);
    EXPECT_EQ(updated.first().toString(), QStringLiteral("palette_recent_5"));
}

TEST(CommandPaletteTest, ExecutesCurrentShortcutsWhileOpen)
{
    ScopedPaletteHistory history;
    ActionRegistry registry;
    registry.add(Action::make(QStringLiteral("palette_shortcut_open"),
                              QStringLiteral("Open Project")));
    registry.add(Action::make(QStringLiteral("palette_shortcut_save"),
                              QStringLiteral("Save Project")));
    registry.add(Action::make(QStringLiteral("palette_shortcut_chord"),
                              QStringLiteral("Show Grid")));
    registry.add(Action::make(QStringLiteral("palette_shortcut_zoom"),
                              QStringLiteral("Zoom In")));
    QAction openAction(QStringLiteral("Open Project"), nullptr);
    openAction.setShortcut(QKeySequence(QStringLiteral("Ctrl+O")));
    registry.find(QStringLiteral("palette_shortcut_open"))
        ->set_qaction(&openAction);
    QAction saveAction(QStringLiteral("Save Project"), nullptr);
    saveAction.setShortcut(QKeySequence(QStringLiteral("Ctrl+S")));
    saveAction.setEnabled(false);
    registry.find(QStringLiteral("palette_shortcut_save"))
        ->set_qaction(&saveAction);
    QAction chordAction(QStringLiteral("Show Grid"), nullptr);
    chordAction.setShortcut(QKeySequence(QStringLiteral("Ctrl+K, Ctrl+M")));
    registry.find(QStringLiteral("palette_shortcut_chord"))
        ->set_qaction(&chordAction);
    QAction zoomAction(QStringLiteral("Zoom In"), nullptr);
    zoomAction.setShortcut(QKeySequence(QStringLiteral("Ctrl++")));
    registry.find(QStringLiteral("palette_shortcut_zoom"))
        ->set_qaction(&zoomAction);

    CommandPalette palette(registry);
    palette.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&palette));
    auto* search = palette.findChild<QLineEdit*>(
        QStringLiteral("commandPaletteSearch"));
    ASSERT_NE(search, nullptr);
    QTest::keyClick(search, Qt::Key_S, Qt::ControlModifier);
    EXPECT_TRUE(palette.isVisible());
    EXPECT_EQ(palette.selected_action(), nullptr);
    QTest::keyClick(search, Qt::Key_K, Qt::ControlModifier);
    QTest::keyClick(search, Qt::Key_M, Qt::ControlModifier);
    EXPECT_EQ(palette.selected_action(), &chordAction);
    EXPECT_EQ(palette.result(), int(QDialog::Accepted));

    CommandPalette second(registry);
    second.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&second));
    auto* secondSearch = second.findChild<QLineEdit*>(
        QStringLiteral("commandPaletteSearch"));
    ASSERT_NE(secondSearch, nullptr);
    QTest::keyClicks(secondSearch, QStringLiteral("grid"));
    EXPECT_EQ(secondSearch->text(), QStringLiteral("grid"));
    QTest::keyClick(secondSearch, Qt::Key_A, Qt::ControlModifier);
    EXPECT_EQ(secondSearch->selectedText(), QStringLiteral("grid"));
    secondSearch->clear();
    QTest::keyClick(secondSearch, Qt::Key_K, Qt::ControlModifier);
    QTest::keyClick(secondSearch, Qt::Key_O, Qt::ControlModifier);
    EXPECT_EQ(second.selected_action(), &openAction);
    EXPECT_EQ(second.result(), int(QDialog::Accepted));

    CommandPalette third(registry);
    third.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&third));
    auto* thirdSearch = third.findChild<QLineEdit*>(
        QStringLiteral("commandPaletteSearch"));
    ASSERT_NE(thirdSearch, nullptr);
    QTest::keyClick(thirdSearch,
                    Qt::Key_Plus,
                    Qt::ControlModifier | Qt::ShiftModifier);
    EXPECT_EQ(third.selected_action(), &zoomAction);
}
