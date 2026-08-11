#pragma once

#include <QDialog>
#include <QList>
#include <QString>
#include <QStringList>

class QComboBox;
class QFileSystemModel;
class QLabel;
class QLineEdit;
class QListWidget;
class QModelIndex;
class QMouseEvent;
class QPushButton;
class QResizeEvent;
class QTreeView;


// Actual filesystem access/iteration is still QFileSystemModel (no
// reason to hand-roll directory listing/sorting/watching, all of which
// it already does correctly) - what's custom here is everything AROUND
// it: the chrome, the address bar + up/quick-access navigation, the
// filter dropdown + filename field, and the accept/overwrite-confirm
// logic each mode (open/open-multi/save/pick-folder) needs.
class FileBrowserDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode {
        OpenFile,
        OpenFiles,
        Save,
        SelectFolder,
    };

    // `nameFilter`: same "Label (*.ext *.ext2);;Label2 (*.ext3)" syntax
    // QFileDialog::setNameFilter() takes - parsed into a dropdown (if
    // more than one entry) and, for Mode::Save, which extension gets
    // appended to a typed name that doesn't already have one (mirrors
    // FileActions's old per-filter fileExt_ map). Empty means "all
    // files", ignored entirely for Mode::SelectFolder.
    FileBrowserDialog(QWidget* parent,
                      Mode mode,
                      const QString& title,
                      const QString& startDir = QString(),
                      const QString& nameFilter = QString(),
                      const QString& defaultFileName = QString());

    // Populated after exec() == QDialog::Accepted - one entry for
    // OpenFile/Save/SelectFolder, one-or-more for OpenFiles.
    QStringList selectedFiles() const { return selected_; }

    // Public only so the free parseNameFilter() helper (file_browser_
    // dialog.cpp, not a member/friend) can return one - not meant as
    // API for callers of this dialog.
    struct FilterEntry
    {
        QString label;
        QStringList patterns;
        QString primaryExt; // "png", empty if the filter has none/isn't a single simple extension
    };

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void setDirectory_(const QString& path);
    void navigateUp_();
    void onDoubleClicked_(const QModelIndex& index);
    void onSelectionChanged_();
    void onFilterChanged_(int index);
    void tryAccept_();
    void buildSidebar_(const QColor& accent);

    Mode mode_;
    QString currentDir_;
    QList<FilterEntry> filters_;
    QStringList selected_;

    QFileSystemModel* model_ = nullptr;
    QTreeView* tree_ = nullptr;
    QListWidget* sidebar_ = nullptr;
    QLineEdit* pathEdit_ = nullptr;
    QPushButton* upBtn_ = nullptr;
    QLineEdit* nameEdit_ = nullptr;
    QComboBox* filterCombo_ = nullptr;
    QPushButton* actionBtn_ = nullptr;
};

// One existing file, or empty if cancelled.
QString showOpenFileDialog(QWidget* parent,
                           const QString& title,
                           const QString& startDir = QString(),
                           const QString& nameFilter = QString());

// One or more existing files, empty list if cancelled.
QStringList showOpenFilesDialog(QWidget* parent,
                                const QString& title,
                                const QString& startDir = QString(),
                                const QString& nameFilter = QString());

// A path to save to (extension auto-appended per the selected filter
// if the typed name didn't already have one; overwrite is confirmed
// internally before returning), or empty if cancelled.
QString showSaveFileDialog(QWidget* parent,
                           const QString& title,
                           const QString& startDir = QString(),
                           const QString& nameFilter = QString(),
                           const QString& defaultFileName = QString());

// A directory path, or empty if cancelled.
QString showSelectFolderDialog(QWidget* parent,
                               const QString& title,
                               const QString& startDir = QString());
