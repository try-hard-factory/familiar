#include "canvasview.h"
#include "file_actions.h"
#include "fml_archive.h"
#include "mainwindow.h"
#include "tabpane.h"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QDialog>
#include <QFile>
#include <QIODevice>
#include <QTemporaryDir>

// FmlArchive::load() repairs a manifest it cannot otherwise open - a
// cyclic group graph, a scale of zero, a crop outside the image - and
// says so in a dialog. Saving afterwards then replaced that same file
// with the repaired content, and nothing kept a copy: whatever the file
// really held was gone, with a dialog at open time the only trace.
//
// The first save over a repaired file now moves the original aside to
// <name>.fml.bak first. These cover when that happens and when it must
// not, not the repairing itself - tests/io/fml_graph_sanitize_test.cpp
// and fml_transform_sanitize_test.cpp do that.

namespace {

const QByteArray koriginalContent = QByteArrayLiteral(
    "not a real .fml - this is the content that has to survive");

class RepairedProjectBackupTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(dir_.isValid());
        window_.tab_pane().add_new_untitled_tab();
        view_ = window_.tab_pane().current_widget();
        ASSERT_NE(view_, nullptr);

        path_ = dir_.filePath(QStringLiteral("project.fml"));
        write_file(path_, koriginalContent);
    }

    static void write_file(const QString& path, const QByteArray& bytes)
    {
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        ASSERT_EQ(file.write(bytes), bytes.size());
    }

    static QByteArray read_file(const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return file.readAll();
    }

    QString backup_of(const QString& path) const
    {
        return path + QStringLiteral(".bak");
    }

    // save_file() falls back to a Save As DIALOG when the target does
    // not exist yet, so every path a test saves to has to exist first.
    int save_over(const QString& path)
    {
        return window_.file_actions().save_file(view_, path);
    }

    MainWindow window_;
    CanvasView* view_ = nullptr;
    QTemporaryDir dir_;
    QString path_;
};

} // namespace

TEST_F(RepairedProjectBackupTest, TheRepairedOriginalIsKeptBesideTheNewFile)
{
    view_->set_repaired_source_path(path_);

    ASSERT_EQ(save_over(path_), QDialog::Accepted);

    EXPECT_EQ(read_file(backup_of(path_)), koriginalContent);

    // And what the save wrote is a real project, not the bytes that
    // just moved out of the way.
    EXPECT_NE(read_file(path_), koriginalContent);
    CanvasScene* scene = view_->scene();
    EXPECT_TRUE(FmlArchive::load(path_, scene).error.isEmpty());
}

// The flag is cleared by that first save, so the second one must not
// overwrite the backup with the repaired content - which would lose the
// original for good, one save later than before.
TEST_F(RepairedProjectBackupTest, TheOriginalIsKeptOnlyOnce)
{
    view_->set_repaired_source_path(path_);
    ASSERT_EQ(save_over(path_), QDialog::Accepted);
    ASSERT_TRUE(view_->repaired_source_path().isEmpty());

    ASSERT_EQ(save_over(path_), QDialog::Accepted);

    EXPECT_EQ(read_file(backup_of(path_)), koriginalContent);
}

// Saving elsewhere does not touch the repaired file, so there is nothing
// to protect and no backup to make - and the flag has to stay, because a
// later save back over that file should still move it aside.
TEST_F(RepairedProjectBackupTest, SavingSomewhereElseLeavesTheOriginalAlone)
{
    const QString other = dir_.filePath(QStringLiteral("elsewhere.fml"));
    write_file(other, QByteArrayLiteral("placeholder"));
    view_->set_repaired_source_path(path_);

    ASSERT_EQ(save_over(other), QDialog::Accepted);

    EXPECT_FALSE(QFile::exists(backup_of(other)));
    EXPECT_FALSE(QFile::exists(backup_of(path_)));
    EXPECT_EQ(read_file(path_), koriginalContent);
    EXPECT_EQ(view_->repaired_source_path(), path_);

    // ... and that later save does move it aside.
    ASSERT_EQ(save_over(path_), QDialog::Accepted);
    EXPECT_EQ(read_file(backup_of(path_)), koriginalContent);
}

TEST_F(RepairedProjectBackupTest, AProjectThatNeededNoRepairGetsNoBackup)
{
    ASSERT_TRUE(view_->repaired_source_path().isEmpty());

    ASSERT_EQ(save_over(path_), QDialog::Accepted);

    EXPECT_FALSE(QFile::exists(backup_of(path_)));
}

// QFile::rename() refuses an existing target, so a leftover .bak has to
// be removed first. Deliberate, and worth pinning: the alternative -
// giving up on the backup because one is already there - would protect
// an older original at the expense of the one about to be destroyed.
TEST_F(RepairedProjectBackupTest, AStaleBackupIsReplacedByTheCurrentOriginal)
{
    write_file(backup_of(path_), QByteArrayLiteral("a much older backup"));
    view_->set_repaired_source_path(path_);

    ASSERT_EQ(save_over(path_), QDialog::Accepted);

    EXPECT_EQ(read_file(backup_of(path_)), koriginalContent);
}
