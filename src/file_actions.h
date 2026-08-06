#ifndef FILEACTIONS_H
#define FILEACTIONS_H

#include <unordered_map>
#include <QFileDialog>
#include <QString>
#include <QUuid>

class TabPane;
class MainWindow;
class CanvasView;

class FileActions
{
public:
    FileActions(MainWindow& mw);
    ~FileActions();

    void newFile();
    void openFile();
    void processOpenFile(const QString& file);

    // Saves `canvasView` specifically, regardless of which tab is
    // currently active - used by autosave to save background tabs
    // without visibly flipping the active tab (see MainWindow::
    // onAutosaveTimeout_()). `path` must already exist on disk (checked
    // by the caller before calling this - never true for an untitled
    // tab); if it doesn't, this falls back to saveFileAs(), which is
    // hard-wired to the *current* tab, so passing a background view with
    // a nonexistent path would silently save the wrong tab instead.
    int saveFile(CanvasView* canvasView, const QString& path);
    int saveFile(const QString& path);
    int saveFile();
    int saveFileAs();

    // Opens a fresh tab (titled at `originalPath` if non-empty, untitled
    // otherwise - see recovery.h's Entry::originalPath) and loads
    // `recoveryFmlPath` into it, marked modified: the recovered content
    // hasn't actually been written back to `originalPath` yet, so the
    // user should notice and consciously save it rather than have it
    // look like an already-saved, unremarkable open file. `recoveryId`
    // (Entry::id) is only deleted from the recovery folder once the
    // (backgrounded) load actually finishes successfully - see
    // loadFmlIntoCurrentTab()'s recoveryIdToClear param below; the
    // caller must NOT delete the recovery file itself right after
    // calling this, since the load hasn't necessarily read it yet.
    void restoreFromRecovery(const QString& recoveryFmlPath,
                             const QString& originalPath,
                             const QUuid& recoveryId);

private:
    // Loads `path` into whatever tab is currently active, in the
    // background (see fml_archive.h). Shared by processOpenFile(),
    // saveFileAs()'s "reload the previous file into a fresh tab" branch,
    // and restoreFromRecovery(). `markModifiedAfterLoad` is only true for
    // the last of those - every other caller wants the normal "freshly
    // opened = clean" behavior. `recoveryIdToClear` (non-null only for
    // restoreFromRecovery()) is removed from the recovery folder from
    // INSIDE the load's finished-callback, once loading has actually
    // succeeded - not by the caller right after starting this, since the
    // load is backgrounded via ThreadedIO and hasn't necessarily opened
    // the source file yet by the time this function returns (a real,
    // previously-shipped bug: deleting it immediately raced the
    // background read and intermittently produced a "No such file or
    // directory" error).
    void loadFmlIntoCurrentTab(const QString& path,
                               bool markModifiedAfterLoad = false,
                               const QUuid& recoveryIdToClear = QUuid());

    MainWindow& mainwindow_;
    std::unordered_map<QString, QString>
        fileExt_; ///< \~english table with file extention
};

#endif // FILEACTIONS_H
