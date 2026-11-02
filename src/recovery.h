#ifndef RECOVERY_H
#define RECOVERY_H

#include <QList>
#include <QString>
#include <QUuid>

class CanvasView;

// Crash recovery: while the app runs, every modified
// tab - including ones never manually saved ("untitled"), which the
// regular periodic autosave deliberately skips since it has no
// real path to save to - gets periodically snapshotted into its own
// pair of files (<id>.fml + <id>.json sidecar) under recoveryDir(). On a
// clean exit these are wiped (see MainWindow's qApp::aboutToQuit
// hookup); if they're still there at the NEXT startup, that means the
// previous session didn't exit cleanly, and MainWindow offers to
// restore or discard them.
namespace familiar::recovery {

struct Entry
{
    QUuid id;
    QString label;        // display name for the recovery dialog
    QString originalPath; // empty if the tab was never manually saved
    QString fmlPath;      // recovery/<id>.fml - the actual content to restore
};

// AppLocalDataLocation/recovery - not guaranteed to exist yet; save()
// creates it on first write.
QString recovery_dir();

// Overwrites this tab's recovery snapshot (content + sidecar metadata).
// Call only for a tab that needs one (CanvasView::
// needs_recovery_snapshot()) - an unmodified one has nothing new to
// protect, and re-writing it on every tick would just be wasted I/O.
//
// Returns as soon as the tab has been READ, which measured at 6ms for a
// 300-picture board. Assembling the archive and writing it - 1.4s for
// that same board, and the whole of the periodic freeze this used to
// cause - happens afterwards on a single background thread shared by
// every tab, one snapshot at a time. A tab whose previous snapshot is
// still in flight is skipped; it stays due one, so the next tick picks
// it up.
//
// The tab is only marked snapshotted once the file is actually on disk,
// and at the revision it was READ at, not the one it has by then - an
// edit made while the write was running leaves it due another snapshot.
void save(CanvasView* canvasView);

// Deletes one tab's recovery files - called when that tab closes while
// the app keeps running, so a stale entry doesn't show up as
// "recoverable" after a later crash in the same run. Also drops a
// snapshot of that tab still in flight, so one cannot reappear right
// after this returns.
void remove(const QUuid& id);

// Stops the background writer for good: no snapshot is accepted
// afterwards. Waits for the one being written right now, if any -
// bounded by a single snapshot, since the queued ones are simply
// dropped.
//
// Call it on the way out, before clear(). clear() on its own waits for
// an in-flight write too, but does not stop the writer, so a timer
// firing between aboutToQuit and the event loop actually stopping would
// put a fresh snapshot into the folder that was just wiped.
void shutdown();

// Wipes the whole recovery folder - called once on a clean app exit.
// By the time the app is actually allowed to quit, every tab's fate
// (saved, or explicitly discarded via the close/Save-All dialogs) has
// already been decided, so nothing left over here still needs
// recovering next launch.
//
// Waits for a snapshot being written right now before wiping, so one
// cannot land in the folder just after it was emptied. Does NOT stop
// the writer - a later save() starts a fresh thread; see shutdown().
void clear();

// True while any tab's snapshot is queued, being written, or written
// but not yet accounted for. Nothing in the app needs to ask - the
// writer settles on its own - it is here so a test can wait for that
// instead of waiting on a duration.
bool is_writing();

// Every leftover entry found at startup - a non-empty result means the
// previous session didn't exit cleanly (crash, kill, power loss).
QList<Entry> scan();

} // namespace familiar::recovery

#endif // RECOVERY_H
