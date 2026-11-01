#include "utils/utils.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QWidget>

#ifdef Q_OS_UNIX
#include <csignal>
#include <sys/resource.h>
#endif

// centered_widget<M, S>() is only ever called with QWidget-derived types
// (MainWindow*, dialog subclasses - see help_dialog.cpp, about_dialog.cpp,
// mainwindow.cpp, ...), all of which share plain QWidget's
// mapToGlobal()/size()/move() for this purpose. Exercised here with bare
// QWidgets rather than one of those concrete types, since the template
// itself doesn't care which - and it's the arithmetic being tested, not
// any dialog-specific behavior.
//
// `expected` below is computed via the SAME parent.mapToGlobal(QPoint(0,
// 0)) call centered_widget() itself makes, not a hardcoded screen
// coordinate - keeps this a check of centered_widget()'s offset math,
// not an assertion about what mapToGlobal() happens to return on
// whatever windowing system this runs under (neither widget is ever
// shown, so no window manager round-trip is involved either way).
TEST(CenteredWidgetTest, CentersWidgetOverParent)
{
    QWidget parent;
    parent.move(50, 50);
    parent.resize(200, 100);

    QWidget child;
    child.resize(50, 40);

    centered_widget(&parent, &child);

    const QPoint expected = parent.mapToGlobal(QPoint(0, 0))
                            + QPoint((200 - 50) / 2, (100 - 40) / 2);
    EXPECT_EQ(child.pos(), expected);
}

TEST(GetRectFromPointsTest, NormalizesRegardlessOfPointOrder)
{
    // Bottom-right-to-top-left drag (point1 "after" point2 on both axes)
    // - the rect it produces should be identical to the top-left-to-
    // bottom-right drag a user more usually makes.
    const QRectF fromReverseDrag = get_rect_from_points(QPointF(100, 80),
                                                        QPointF(10, 20));
    const QRectF fromForwardDrag = get_rect_from_points(QPointF(10, 20),
                                                        QPointF(100, 80));

    EXPECT_EQ(fromReverseDrag, fromForwardDrag);
    EXPECT_EQ(fromForwardDrag, QRectF(QPointF(10, 20), QPointF(100, 80)));
}

TEST(GetRectFromPointsTest, ZeroSizeWhenPointsCoincide)
{
    const QRectF r = get_rect_from_points(QPointF(5, 5), QPointF(5, 5));
    EXPECT_TRUE(r.isEmpty());
}

TEST(RoundToTest, RoundsToNearestMultiple)
{
    EXPECT_DOUBLE_EQ(round_to(23.0, 10.0), 20.0);
    EXPECT_DOUBLE_EQ(round_to(27.0, 10.0), 30.0);
    EXPECT_DOUBLE_EQ(round_to(0.0, 10.0), 0.0);
}

TEST(RoundToTest, NegativeNumbers)
{
    EXPECT_DOUBLE_EQ(round_to(-23.0, 10.0), -20.0);
}

// ── write_file_atomically() ───────────────────────────────────────────────────
// The property under test is not "the bytes arrive" (any QFile does
// that) but "the file that was already there is never the casualty of a
// write that didn't finish". settings.json and the crash-recovery
// sidecar both used plain QFile with QIODevice::Truncate, which destroys
// the old contents at open() time and reports nothing when the write
// then fails - see write_file_atomically()'s own header comment.

namespace {

QByteArray read_all(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

} // namespace

TEST(WriteFileAtomicallyTest, CreatesAFileThatDoesNotExistYet)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("new.json"));

    EXPECT_TRUE(write_file_atomically(path, QByteArray("{\"a\":1}")));
    EXPECT_EQ(read_all(path), QByteArray("{\"a\":1}"));
}

TEST(WriteFileAtomicallyTest, ReplacesExistingContentsWholesale)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("x.json"));

    ASSERT_TRUE(write_file_atomically(path, QByteArray("aaaaaaaaaaaaaaaa")));
    // Shorter than what's already there: the leftover tail of the old
    // contents must not survive (QSaveFile replaces the whole file
    // rather than overwriting in place, but pin it anyway).
    ASSERT_TRUE(write_file_atomically(path, QByteArray("b")));
    EXPECT_EQ(read_all(path), QByteArray("b"));
}

TEST(WriteFileAtomicallyTest, FailsAndKeepsTheOldFileWhenTheTargetIsUnwritable)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("x.json"));
    ASSERT_TRUE(write_file_atomically(path, QByteArray("GOOD")));

    // QSaveFile needs to create its temporary file in the target's own
    // directory, so a directory it cannot write to fails the open() -
    // before anything has touched the real file.
    QFile dirAsFile(dir.path());
    ASSERT_TRUE(dirAsFile.setPermissions(QFileDevice::ReadOwner
                                         | QFileDevice::ExeOwner));
    const QString probe = QDir(dir.path()).filePath(QStringLiteral("probe"));
    QFile probeFile(probe);
    const bool dirIsStillWritable = probeFile.open(QIODevice::WriteOnly);
    probeFile.close();
    QFile::remove(probe);

    if (dirIsStillWritable) {
        // Running as root (CI containers often do) - permissions don't
        // apply, so there is nothing to assert here.
        dirAsFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                 | QFileDevice::ExeOwner);
        GTEST_SKIP() << "directory permissions are not enforced for this user";
    }

    EXPECT_FALSE(write_file_atomically(path, QByteArray("NEW")));
    EXPECT_EQ(read_all(path), QByteArray("GOOD"));

    // QTemporaryDir cannot clean up a directory it may not write to.
    dirAsFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                             | QFileDevice::ExeOwner);
}

#ifdef Q_OS_UNIX
TEST(WriteFileAtomicallyTest, FailsAndKeepsTheOldFileWhenTheWriteIsCutShort)
{
    // The regression test for the real defect, with RLIMIT_FSIZE
    // standing in for a full disk or an exceeded quota: the only
    // failure mode of this kind that can be injected deterministically.
    //
    // Note where the failure actually surfaces: QFile/QSaveFile buffer,
    // so write() reports every byte accepted and only the flush inside
    // commit() fails. That is exactly why the old code could not detect
    // this - it checked nothing, but even checking write() would not
    // have been enough.
    //
    // The cap is deliberately megabytes rather than bytes: it applies to
    // every file this process writes, the log file among them, and a
    // test must not be able to break logging for the rest of the suite.
    constexpr rlim_t klimitBytes = 4u * 1024 * 1024;
    constexpr int kpayloadBytes = 8 * 1024 * 1024;

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("x.json"));
    ASSERT_TRUE(write_file_atomically(path, QByteArray("GOOD")));

    rlimit previous{};
    ASSERT_EQ(getrlimit(RLIMIT_FSIZE, &previous), 0);
    if (previous.rlim_max < klimitBytes) {
        GTEST_SKIP() << "RLIMIT_FSIZE hard limit is already below the cap";
    }
    // Exceeding RLIMIT_FSIZE raises SIGXFSZ as well as failing the
    // write, and its default action terminates the process.
    void* const previousHandler = reinterpret_cast<void*>(
        signal(SIGXFSZ, SIG_IGN));
    const rlimit capped{klimitBytes, previous.rlim_max};
    ASSERT_EQ(setrlimit(RLIMIT_FSIZE, &capped), 0);

    const bool wrote = write_file_atomically(path,
                                             QByteArray(kpayloadBytes, 'x'));

    ASSERT_EQ(setrlimit(RLIMIT_FSIZE, &previous), 0);
    signal(SIGXFSZ, reinterpret_cast<sighandler_t>(previousHandler));

    EXPECT_FALSE(wrote);
    EXPECT_EQ(read_all(path), QByteArray("GOOD"));
}

TEST(WriteFileAtomicallyTest, KeepsTheExistingFilesPermissions)
{
    // QSaveFile creates its temporary file 0600 and copies the target's
    // mode over on commit. Verified here because settings.json is
    // rewritten on every settings change: silently tightening (or
    // loosening) its mode on each write would be a real regression.
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("x.json"));
    ASSERT_TRUE(write_file_atomically(path, QByteArray("GOOD")));

    QFile file(path);
    ASSERT_TRUE(file.setPermissions(QFileDevice::ReadOwner
                                    | QFileDevice::WriteOwner
                                    | QFileDevice::ReadGroup));
    const QFileDevice::Permissions before = QFile(path).permissions();

    ASSERT_TRUE(write_file_atomically(path, QByteArray("NEWER")));

    EXPECT_EQ(QFile(path).permissions(), before);
}
#endif // Q_OS_UNIX
