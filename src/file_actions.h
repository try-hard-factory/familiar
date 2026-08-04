#ifndef FILEACTIONS_H
#define FILEACTIONS_H

#include <unordered_map>
#include <QFileDialog>
#include <QString>

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

private:
    // Loads `path` into whatever tab is currently active, in the
    // background (see fml_archive.h). Shared by processOpenFile() and
    // saveFileAs()'s "reload the previous file into a fresh tab" branch.
    void loadFmlIntoCurrentTab(const QString& path);

    MainWindow& mainwindow_;
    std::unordered_map<QString, QString>
        fileExt_; ///< \~english table with file extention
};

#endif // FILEACTIONS_H
