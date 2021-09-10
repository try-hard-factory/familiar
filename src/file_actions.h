#ifndef FILEACTIONS_H
#define FILEACTIONS_H

#include <unordered_map>
#include <QString>
#include <QFileDialog>

class TabPane;
class MainWindow;

class FileActions
{
public:
    FileActions(MainWindow& mw);
    ~FileActions();

    void newFile();
    void openFile();
    void processOpenFile(QString file);

    int saveFile(QString path);
    int saveFile();
    int saveFileAs();
private:
    MainWindow& mainwindow_;
    std::unordered_map<QString, QString> fileExt_; ///< \~english table with file extention \~russian таблица с расширениями файлов
};

#endif // FILEACTIONS_H
