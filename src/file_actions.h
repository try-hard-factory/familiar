#ifndef FILEACTIONS_H
#define FILEACTIONS_H

#include <unordered_map>
#include <QFileDialog>
#include <QString>

class TabPane;
class MainWindow;

class FileActions
{
public:
    FileActions(MainWindow& mw);
    ~FileActions();

    void newFile();
    void openFile();
    void processOpenFile(const QString& file);

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
