#include "widgets/file_browser_dialog.h"

#include <gtest/gtest.h>

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileSystemModel>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QTemporaryDir>
#include <QTreeView>

namespace {

class FileBrowserDialogTest : public ::testing::Test
{
protected:
    QPushButton* button(FileBrowserDialog& dialog, const QString& label)
    {
        for (auto* candidate : dialog.findChildren<QPushButton*>()) {
            if (candidate->text() == label) {
                return candidate;
            }
        }
        return nullptr;
    }

    QLineEdit* filename_edit(FileBrowserDialog& dialog)
    {
        const auto edits = dialog.findChildren<QLineEdit*>(
            QString(), Qt::FindDirectChildrenOnly);
        return edits.size() == 2 ? edits.at(1) : nullptr;
    }

    static bool write_file(const QString& path)
    {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write("data") == 4;
    }

    QTemporaryDir dir_;
};

TEST_F(FileBrowserDialogTest, SaveAppendsExtensionFromSelectedFilter)
{
    ASSERT_TRUE(dir_.isValid());
    FileBrowserDialog dialog(nullptr, FileBrowserDialog::Mode::Save,
                             QStringLiteral("Save"), dir_.path(),
                             QStringLiteral("Familiar (*.fml);;PNG (*.png)"));
    auto* edit = filename_edit(dialog);
    auto* save = button(dialog, QStringLiteral("Save"));
    auto* filters = dialog.findChild<QComboBox*>();
    ASSERT_NE(edit, nullptr);
    ASSERT_NE(save, nullptr);
    ASSERT_NE(filters, nullptr);

    filters->setCurrentIndex(1);
    edit->setText(QStringLiteral("island"));
    save->click();

    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(dialog.selected_files(),
              QStringList{dir_.filePath(QStringLiteral("island.png"))});
}

TEST_F(FileBrowserDialogTest, SaveKeepsAnExplicitExtension)
{
    FileBrowserDialog dialog(nullptr, FileBrowserDialog::Mode::Save,
                             QStringLiteral("Save"), dir_.path(),
                             QStringLiteral("Familiar (*.fml)"));
    auto* edit = filename_edit(dialog);
    auto* save = button(dialog, QStringLiteral("Save"));
    ASSERT_NE(edit, nullptr);
    ASSERT_NE(save, nullptr);

    edit->setText(QStringLiteral("island.backup"));
    save->click();

    EXPECT_EQ(dialog.selected_files(),
              QStringList{dir_.filePath(QStringLiteral("island.backup"))});
}

TEST_F(FileBrowserDialogTest, SaveRejectsBlankName)
{
    FileBrowserDialog dialog(nullptr, FileBrowserDialog::Mode::Save,
                             QStringLiteral("Save"), dir_.path());
    auto* edit = filename_edit(dialog);
    auto* save = button(dialog, QStringLiteral("Save"));
    ASSERT_NE(edit, nullptr);
    ASSERT_NE(save, nullptr);

    edit->setText(QStringLiteral("  "));
    save->click();

    EXPECT_NE(dialog.result(), QDialog::Accepted);
    EXPECT_TRUE(dialog.selected_files().isEmpty());
}

TEST_F(FileBrowserDialogTest, OpenFileAcceptsOnlyASelectedFile)
{
    const QString path = dir_.filePath(QStringLiteral("board.fml"));
    ASSERT_TRUE(write_file(path));
    FileBrowserDialog dialog(nullptr, FileBrowserDialog::Mode::OpenFile,
                             QStringLiteral("Open"), dir_.path());
    auto* open = button(dialog, QStringLiteral("Open"));
    auto* tree = dialog.findChild<QTreeView*>();
    ASSERT_NE(open, nullptr);
    ASSERT_NE(tree, nullptr);

    open->click();
    EXPECT_TRUE(dialog.selected_files().isEmpty());
    auto* model = qobject_cast<QFileSystemModel*>(tree->model());
    ASSERT_NE(model, nullptr);
    const QModelIndex index = model->index(path);
    ASSERT_TRUE(index.isValid());
    tree->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect
                                             | QItemSelectionModel::Rows);
    open->click();

    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(dialog.selected_files(), QStringList{path});
}

TEST_F(FileBrowserDialogTest, OpenFilesReturnsAllSelectedFiles)
{
    const QString first = dir_.filePath(QStringLiteral("a.fml"));
    const QString second = dir_.filePath(QStringLiteral("b.fml"));
    ASSERT_TRUE(write_file(first));
    ASSERT_TRUE(write_file(second));
    FileBrowserDialog dialog(nullptr, FileBrowserDialog::Mode::OpenFiles,
                             QStringLiteral("Open"), dir_.path());
    auto* open = button(dialog, QStringLiteral("Open"));
    auto* tree = dialog.findChild<QTreeView*>();
    ASSERT_NE(open, nullptr);
    ASSERT_NE(tree, nullptr);
    auto* model = qobject_cast<QFileSystemModel*>(tree->model());
    ASSERT_NE(model, nullptr);
    const QModelIndex firstIndex = model->index(first);
    const QModelIndex secondIndex = model->index(second);
    ASSERT_TRUE(firstIndex.isValid());
    ASSERT_TRUE(secondIndex.isValid());

    tree->selectionModel()->select(firstIndex, QItemSelectionModel::ClearAndSelect
                                                  | QItemSelectionModel::Rows);
    tree->selectionModel()->select(secondIndex, QItemSelectionModel::Select
                                                   | QItemSelectionModel::Rows);
    open->click();

    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    const QStringList selected = dialog.selected_files();
    EXPECT_EQ(QSet<QString>(selected.begin(), selected.end()),
              QSet<QString>({first, second}));
}

TEST_F(FileBrowserDialogTest, SelectFolderUsesCurrentDirectoryWithoutSelection)
{
    FileBrowserDialog dialog(nullptr, FileBrowserDialog::Mode::SelectFolder,
                             QStringLiteral("Choose"), dir_.path());
    auto* choose = button(dialog, QStringLiteral("Select Folder"));
    ASSERT_NE(choose, nullptr);

    choose->click();

    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(dialog.selected_files(), QStringList{dir_.path()});
}

TEST_F(FileBrowserDialogTest, SelectFolderUsesSelectedSubdirectory)
{
    const QString child = dir_.filePath(QStringLiteral("child"));
    ASSERT_TRUE(QDir().mkpath(child));
    FileBrowserDialog dialog(nullptr, FileBrowserDialog::Mode::SelectFolder,
                             QStringLiteral("Choose"), dir_.path());
    auto* choose = button(dialog, QStringLiteral("Select Folder"));
    auto* tree = dialog.findChild<QTreeView*>();
    ASSERT_NE(choose, nullptr);
    ASSERT_NE(tree, nullptr);
    auto* model = qobject_cast<QFileSystemModel*>(tree->model());
    ASSERT_NE(model, nullptr);
    const QModelIndex index = model->index(child);
    ASSERT_TRUE(index.isValid());

    tree->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect
                                             | QItemSelectionModel::Rows);
    choose->click();

    EXPECT_EQ(dialog.selected_files(), QStringList{child});
}

TEST_F(FileBrowserDialogTest, NewFolderChoosesAnUnusedName)
{
    ASSERT_TRUE(QDir().mkpath(dir_.filePath(QStringLiteral("New Folder"))));
    FileBrowserDialog dialog(nullptr, FileBrowserDialog::Mode::SelectFolder,
                             QStringLiteral("Choose"), dir_.path());
    auto* create = button(dialog, QStringLiteral("New Folder"));
    ASSERT_NE(create, nullptr);

    create->click();

    EXPECT_TRUE(QDir(dir_.filePath(QStringLiteral("New Folder (2)"))).exists());
}

TEST_F(FileBrowserDialogTest, ChangingFilterUpdatesVisibleFilePatterns)
{
    FileBrowserDialog dialog(nullptr, FileBrowserDialog::Mode::OpenFile,
                             QStringLiteral("Open"), dir_.path(),
                             QStringLiteral("Projects (*.fml);;Images (*.png *.jpg)"));
    auto* filters = dialog.findChild<QComboBox*>();
    auto* tree = dialog.findChild<QTreeView*>();
    ASSERT_NE(tree, nullptr);
    auto* model = qobject_cast<QFileSystemModel*>(tree->model());
    ASSERT_NE(model, nullptr);
    ASSERT_NE(filters, nullptr);

    EXPECT_EQ(model->nameFilters(), QStringList{QStringLiteral("*.fml")});
    filters->setCurrentIndex(1);
    EXPECT_EQ(model->nameFilters(),
              (QStringList{QStringLiteral("*.png"), QStringLiteral("*.jpg")}));
}

} // namespace
