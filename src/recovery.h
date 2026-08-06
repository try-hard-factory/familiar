#ifndef RECOVERY_H
#define RECOVERY_H

#include <QList>
#include <QString>
#include <QUuid>

class CanvasView;

// Crash recovery (roadmap step 18): while the app runs, every modified
// tab - including ones never manually saved ("untitled"), which the
// regular periodic autosave (step 13) deliberately skips since it has no
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
    QString label; // display name for the recovery dialog
    QString originalPath; // empty if the tab was never manually saved
    QString fmlPath; // recovery/<id>.fml - the actual content to restore
};

// AppLocalDataLocation/recovery - not guaranteed to exist yet; save()
// creates it on first write.
QString recoveryDir();

// Overwrites this tab's recovery snapshot (content + sidecar metadata).
// Call only for a modified tab - an unmodified one has nothing new to
// protect, and re-writing it on every tick would just be wasted I/O.
void save(CanvasView* canvasView);

// Deletes one tab's recovery files - called when that tab closes while
// the app keeps running, so a stale entry doesn't show up as
// "recoverable" after a later crash in the same run.
void remove(const QUuid& id);

// Wipes the whole recovery folder - called once on a clean app exit.
// By the time the app is actually allowed to quit, every tab's fate
// (saved, or explicitly discarded via the close/Save-All dialogs) has
// already been decided, so nothing left over here still needs
// recovering next launch.
void clear();

// Every leftover entry found at startup - a non-empty result means the
// previous session didn't exit cleanly (crash, kill, power loss).
QList<Entry> scan();

} // namespace familiar::recovery

#endif // RECOVERY_H
