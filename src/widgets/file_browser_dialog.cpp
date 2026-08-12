#include "file_browser_dialog.h"
#include "dialog_style.h"
#include "message_box.h"

#include <core/settingshandler.h>

#include <QAction>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFont>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QShortcut>
#include <QStandardPaths>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWindow>

#include "utils/utils.h"

namespace {

// Same reasoning as FileActions' own (private, file_actions.cpp)
// FmlIconProvider - no OS-level icon association for .fml is
// registered (packaging concern, out of scope - docs/
// fml_format_design.en.md), so this app's own logo stands in for it
// here too, on every file browser instance now instead of just the
// two dialogs that used to build one locally.
class FmlIconProvider : public QFileIconProvider
{
public:
    QIcon icon(const QFileInfo& info) const override
    {
        if (info.suffix().compare(QStringLiteral("fml"), Qt::CaseInsensitive)
            == 0) {
            static const QIcon fmlIcon(
                QStringLiteral(":/img/app/familiar_256.png"));
            return fmlIcon;
        }
        return QFileIconProvider::icon(info);
    }
};

QList<FileBrowserDialog::FilterEntry> parseNameFilter(const QString& filterString)
{
    QList<FileBrowserDialog::FilterEntry> entries;
    const QStringList parts = filterString.isEmpty()
                                  ? QStringList{QObject::tr("All Files (*)")}
                                  : filterString.split(QStringLiteral(";;"),
                                                       Qt::SkipEmptyParts);

    for (const QString& part : parts) {
        FileBrowserDialog::FilterEntry entry;
        const QString trimmed = part.trimmed();
        const int openParen = trimmed.indexOf(QLatin1Char('('));
        const int closeParen = trimmed.lastIndexOf(QLatin1Char(')'));
        if (openParen >= 0 && closeParen > openParen) {
            entry.label = trimmed;
            const QString patternsStr = trimmed.mid(openParen + 1,
                                                    closeParen - openParen - 1);
            entry.patterns = patternsStr.split(QLatin1Char(' '),
                                               Qt::SkipEmptyParts);
        } else {
            entry.label = trimmed;
            entry.patterns = {QStringLiteral("*")};
        }
        if (!entry.patterns.isEmpty()) {
            const QString& first = entry.patterns.first();
            if (first.startsWith(QStringLiteral("*.")))
                entry.primaryExt = first.mid(2);
        }
        entries.append(entry);
    }
    return entries;
}

} // namespace

FileBrowserDialog::FileBrowserDialog(QWidget* parent,
                                     Mode mode,
                                     const QString& title,
                                     const QString& startDir,
                                     const QString& nameFilter,
                                     const QString& defaultFileName)
    : QDialog(parent)
    , mode_(mode)
{
    filters_ = parseNameFilter(nameFilter);

    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_StyledBackground);
    setWindowModality(Qt::ApplicationModal);
    setWindowTitle(title);
    setFixedSize(700, 460);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 150));
    setGraphicsEffect(shadow);

    auto colorPreset = SettingsHandler::getInstance()->getCurrentColorPreset();
    const QColor& textColor = colorPreset[EPresetsColorIdx::kTextColor];
    const QColor& background = colorPreset[EPresetsColorIdx::kBackgroundColor];
    const QColor& border = colorPreset[EPresetsColorIdx::kBorderColor];
    const QColor& accent = colorPreset[EPresetsColorIdx::kSelectionColor];

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 12, 16, 16);
    outer->setSpacing(8);

    // ── Title row ────────────────────────────────────────────────────
    auto* topRow = new QHBoxLayout();
    auto* titleLabel = new QLabel(title, this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    topRow->addWidget(titleLabel, 1);

    auto* closeBtn = new QPushButton(QStringLiteral("×"), this);
    closeBtn->setFixedSize(22, 22);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFocusPolicy(Qt::NoFocus);
    closeBtn->setObjectName(QStringLiteral("fbdCloseBtn"));
    connect(closeBtn, &QPushButton::clicked, this, &FileBrowserDialog::reject);
    topRow->addWidget(closeBtn);
    outer->addLayout(topRow);

    // ── Address bar ──────────────────────────────────────────────────
    auto* navRow = new QHBoxLayout();
    upBtn_ = new QPushButton(QStringLiteral("↑"), this);
    upBtn_->setFixedSize(28, 28);
    upBtn_->setCursor(Qt::PointingHandCursor);
    upBtn_->setToolTip(tr("Up one level"));
    connect(upBtn_,
            &QPushButton::clicked,
            this,
            &FileBrowserDialog::navigateUp_);
    navRow->addWidget(upBtn_);

    pathEdit_ = new QLineEdit(this);
    connect(pathEdit_, &QLineEdit::editingFinished, this, [this] {
        setDirectory_(pathEdit_->text());
    });
    navRow->addWidget(pathEdit_, 1);

    newFolderBtn_ = new QPushButton(tr("New Folder"), this);
    newFolderBtn_->setCursor(Qt::PointingHandCursor);
    connect(newFolderBtn_,
            &QPushButton::clicked,
            this,
            &FileBrowserDialog::createFolder_);
    navRow->addWidget(newFolderBtn_);
    outer->addLayout(navRow);

    // ── Sidebar + tree ───────────────────────────────────────────────
    auto* contentRow = new QHBoxLayout();
    contentRow->setSpacing(10);

    buildSidebar_(accent);
    contentRow->addWidget(sidebar_);

    model_ = new QFileSystemModel(this);
    model_->setRootPath(QDir::rootPath());
    static FmlIconProvider iconProvider;
    model_->setIconProvider(&iconProvider);
    // QFileSystemModel defaults to read-only, which makes flags() never
    // set Qt::ItemIsEditable regardless of actual filesystem permissions
    // - createFolder_()'s tree_->edit() call would silently no-op (Qt
    // logs "edit: editing failed") without this. NoEditTriggers above
    // still blocks the user from triggering rename via double-click/F2
    // on their own - this only unblocks our own programmatic edit().
    model_->setReadOnly(false);
    if (mode_ == Mode::SelectFolder) {
        model_->setFilter(QDir::AllDirs | QDir::Drives | QDir::NoDotAndDotDot);
    } else {
        model_->setFilter(QDir::AllDirs | QDir::Files | QDir::Drives
                          | QDir::NoDotAndDotDot);
        if (!filters_.isEmpty())
            model_->setNameFilters(filters_.first().patterns);
        model_->setNameFilterDisables(false);
    }

    tree_ = new QTreeView(this);
    tree_->setModel(model_);
    tree_->setColumnHidden(2, true); // Type - Name/Size/Date Modified is plenty
    tree_->setColumnWidth(0, 300);
    tree_->setRootIsDecorated(false);
    tree_->setSortingEnabled(true);
    tree_->sortByColumn(0, Qt::AscendingOrder);
    tree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree_->setSelectionMode(mode_ == Mode::OpenFiles
                                ? QAbstractItemView::ExtendedSelection
                                : QAbstractItemView::SingleSelection);
    tree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(tree_,
            &QTreeView::doubleClicked,
            this,
            &FileBrowserDialog::onDoubleClicked_);
    connect(tree_->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this,
            &FileBrowserDialog::onSelectionChanged_);
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tree_,
            &QTreeView::customContextMenuRequested,
            this,
            &FileBrowserDialog::showContextMenu_);
    // Scoped to tree_ (WidgetWithChildrenShortcut) rather than the whole
    // dialog, so these don't fire while e.g. typing in nameEdit_ (Save
    // mode's filename field) or pathEdit_. Delete doesn't need a guard
    // against firing mid-rename - QLineEdit already claims Delete/
    // Backspace/etc. for itself via QEvent::ShortcutOverride before any
    // QShortcut gets a look, which is exactly why a plain QLineEdit
    // editing session never loses characters to an app-level Delete
    // shortcut elsewhere.
    auto* deleteShortcut = new QShortcut(QKeySequence::Delete, tree_);
    deleteShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(deleteShortcut,
            &QShortcut::activated,
            this,
            &FileBrowserDialog::deleteSelected_);
    auto* renameShortcut = new QShortcut(QKeySequence(Qt::Key_F2), tree_);
    renameShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(renameShortcut,
            &QShortcut::activated,
            this,
            &FileBrowserDialog::renameSelected_);
    contentRow->addWidget(tree_, 1);
    outer->addLayout(contentRow, 1);

    // ── Filename field (Save only) ──────────────────────────────────
    if (mode_ == Mode::Save) {
        nameEdit_ = new QLineEdit(this);
        if (!defaultFileName.isEmpty())
            nameEdit_->setText(defaultFileName);
        connect(nameEdit_,
                &QLineEdit::returnPressed,
                this,
                &FileBrowserDialog::tryAccept_);
        outer->addWidget(nameEdit_);
    }

    if (mode_ != Mode::SelectFolder) {
        filterCombo_ = new QComboBox(this);
        for (const FilterEntry& f : filters_)
            filterCombo_->addItem(f.label);
        connect(filterCombo_,
                qOverload<int>(&QComboBox::currentIndexChanged),
                this,
                &FileBrowserDialog::onFilterChanged_);
        outer->addWidget(filterCombo_);
    }

    // ── Buttons ──────────────────────────────────────────────────────
    auto* buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(8);
    buttonRow->addStretch();
    auto* cancelBtn = new QPushButton(tr("Cancel"), this);
    familiar::dialog_style::styleSecondaryButton(cancelBtn, textColor, border);
    connect(cancelBtn, &QPushButton::clicked, this, &FileBrowserDialog::reject);
    buttonRow->addWidget(cancelBtn);

    const QString actionLabel = mode_ == Mode::Save ? tr("Save")
                                : mode_ == Mode::SelectFolder
                                    ? tr("Select Folder")
                                    : tr("Open");
    actionBtn_ = new QPushButton(actionLabel, this);
    familiar::dialog_style::stylePrimaryButton(actionBtn_, accent);
    actionBtn_->setDefault(true);
    connect(actionBtn_,
            &QPushButton::clicked,
            this,
            &FileBrowserDialog::tryAccept_);
    buttonRow->addWidget(actionBtn_);
    outer->addLayout(buttonRow);

    setStyleSheet(familiar::dialog_style::panelStyleSheet("FileBrowserDialog",
                                                          background,
                                                          border,
                                                          textColor)
                  + familiar::dialog_style::closeButtonStyleSheet("fbdCloseBtn",
                                                                  textColor,
                                                                  accent)
                  + QStringLiteral(
                        "QLineEdit, QComboBox {"
                        "  background-color: rgba(0,0,0,20);"
                        "  color: %1;"
                        "  border: 1px solid %2;"
                        "  border-radius: 4px;"
                        "  padding: 4px 6px;"
                        "}"
                        "QTreeView, QListWidget {"
                        "  background-color: rgba(0,0,0,12);"
                        "  color: %1;"
                        "  border: 1px solid %2;"
                        "  border-radius: 4px;"
                        "}"
                        "QTreeView::item:selected, QListWidget::item:selected {"
                        "  background-color: %3;"
                        "  color: white;"
                        "}"
                        "QHeaderView::section {"
                        "  background-color: transparent;"
                        "  color: %1;"
                        "  border: none;"
                        "  border-bottom: 1px solid %2;"
                        "  padding: 4px;"
                        "}"
                        // createFolder_()'s inline-rename editor is a
                        // QLineEdit sitting directly on top of the
                        // still-selected (red-highlighted) row - the
                        // generic QLineEdit rule above is deliberately
                        // near-transparent (rgba alpha 20) to read as a
                        // recessed field over the dialog's own plain
                        // background, but layered over an already-
                        // painted row it barely masked the OLD text
                        // underneath, showing both at once. This more
                        // specific selector (QTreeView descendant) wins
                        // over the generic one and forces a fully opaque
                        // backing just for this editor.
                        "QTreeView QLineEdit {"
                        "  background-color: %4;"
                        "  color: %1;"
                        "  border: 1px solid %3;"
                        "  border-radius: 2px;"
                        "  padding: 0px 2px;"
                        "  selection-background-color: %3;"
                        "  selection-color: white;"
                        "}")
                        .arg(textColor.name(),
                             border.name(),
                             accent.name(),
                             QColor(background.red(),
                                    background.green(),
                                    background.blue())
                                 .name()));

    setDirectory_(startDir.isEmpty() ? QDir::homePath() : startDir);
}

void FileBrowserDialog::buildSidebar_(const QColor& accent)
{
    Q_UNUSED(accent);
    sidebar_ = new QListWidget(this);
    sidebar_->setFixedWidth(130);
    sidebar_->setFrameShape(QFrame::NoFrame);

    auto addEntry = [this](const QString& label, const QString& path) {
        if (path.isEmpty() || !QDir(path).exists())
            return;
        auto* item = new QListWidgetItem(label, sidebar_);
        item->setData(Qt::UserRole, path);
    };
    addEntry(tr("Home"),
             QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    addEntry(tr("Desktop"),
             QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
    addEntry(tr("Documents"),
             QStandardPaths::writableLocation(
                 QStandardPaths::DocumentsLocation));
    addEntry(tr("Root"), QDir::rootPath());

    connect(sidebar_,
            &QListWidget::itemClicked,
            this,
            [this](QListWidgetItem* item) {
                setDirectory_(item->data(Qt::UserRole).toString());
            });
}

void FileBrowserDialog::setDirectory_(const QString& path)
{
    QFileInfo info(path);
    const QString dirPath = info.isDir() ? info.absoluteFilePath()
                                         : info.absolutePath();
    QDir dir(dirPath);
    if (!dir.exists())
        return;
    currentDir_ = dir.absolutePath();
    tree_->setRootIndex(model_->index(currentDir_));
    pathEdit_->setText(currentDir_);
    QDir parentCheck(currentDir_);
    upBtn_->setEnabled(parentCheck.cdUp());
}

void FileBrowserDialog::navigateUp_()
{
    QDir dir(currentDir_);
    if (dir.cdUp())
        setDirectory_(dir.absolutePath());
}

void FileBrowserDialog::createFolder_()
{
    QDir dir(currentDir_);
    QString name = tr("New Folder");
    for (int suffix = 2; dir.exists(name); ++suffix)
        name = tr("New Folder (%1)").arg(suffix);
    if (!dir.mkdir(name))
        return;

    // Same "create, then rename in place" flow as a real file manager -
    // no separate name-prompt dialog needed (this app has no custom
    // text-input dialog to reuse, and a native QInputDialog would be the
    // one native dialog left in an otherwise fully custom set - see
    // widgets/dialog_style.h's siblings). QFileSystemModel::index(path)
    // resolves synchronously against the filesystem rather than waiting
    // on its change-watcher, so the freshly created row is already there
    // by the time edit() is called. edit() itself ignores editTriggers_
    // (that only gates user-initiated editing, e.g. F2/double-click) -
    // NoEditTriggers above stays in effect for every OTHER row.
    const QModelIndex idx = model_->index(dir.filePath(name));
    if (!idx.isValid())
        return;
    tree_->setCurrentIndex(idx);
    tree_->scrollTo(idx);
    tree_->edit(idx);
}

void FileBrowserDialog::renameSelected_()
{
    const auto rows = tree_->selectionModel()->selectedRows();
    if (rows.size() != 1)
        return;
    tree_->edit(rows.first());
}

void FileBrowserDialog::deleteSelected_()
{
    const auto rows = tree_->selectionModel()->selectedRows();
    if (rows.isEmpty())
        return;

    QStringList paths;
    for (const QModelIndex& idx : rows)
        paths << model_->filePath(idx);

    // Permanent, not a trash move - QFile::moveToTrash()'s static
    // overload needs a newer Qt than this project targets (see
    // CMakeLists.txt) - so this asks first, same as tryAccept_()'s own
    // overwrite confirmation just below.
    const QString message
        = paths.size() == 1
              ? tr("Permanently delete \"%1\"?")
                    .arg(QFileInfo(paths.first()).fileName())
              : tr("Permanently delete %1 items?").arg(paths.size());
    const auto reply = showMessageBox(QMessageBox::Warning,
                                      this,
                                      tr("Delete?"),
                                      message,
                                      QMessageBox::Yes | QMessageBox::No,
                                      QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    for (const QString& path : paths) {
        const QFileInfo info(path);
        if (info.isDir())
            QDir(path).removeRecursively();
        else
            QFile::remove(path);
    }
}

void FileBrowserDialog::showContextMenu_(const QPoint& pos)
{
    const QModelIndex idx = tree_->indexAt(pos);
    if (!idx.isValid()) {
        // Empty area - a stale prior selection shouldn't be what
        // Rename/Delete below silently act on.
        tree_->clearSelection();
    } else if (!tree_->selectionModel()->isSelected(idx)) {
        // Right-clicking an item outside the current selection replaces
        // it, same as a plain left click would - right-clicking INSIDE
        // an existing multi-selection keeps it intact (so Delete can
        // still act on all of it).
        tree_->selectionModel()->select(idx,
                                        QItemSelectionModel::ClearAndSelect
                                            | QItemSelectionModel::Rows);
        tree_->setCurrentIndex(idx);
    }
    const auto rows = tree_->selectionModel()->selectedRows();

    // QMenu gets none of this dialog's own styleSheet() - it's a
    // separate top-level popup, not a plain child widget - so left
    // unstyled it falls back to the app-wide "background: transparent"
    // cascade (MainWindow's own setStyleSheet()) with nothing underneath
    // to actually paint, rendering solid black. Same root cause as
    // dialog_style::panelStyleSheet()'s QToolTip rule, just for a widget
    // type that rule doesn't cover.
    const auto colorPreset = SettingsHandler::getInstance()
                                 ->getCurrentColorPreset();
    const QColor& menuBg = colorPreset[EPresetsColorIdx::kBackgroundColor];
    const QColor& menuBorder = colorPreset[EPresetsColorIdx::kBorderColor];
    const QColor& menuText = colorPreset[EPresetsColorIdx::kTextColor];
    const QColor& menuAccent = colorPreset[EPresetsColorIdx::kSelectionColor];

    QMenu menu(this);
    menu.setStyleSheet(
        QStringLiteral("QMenu {"
                       "  background-color: %1;"
                       "  color: %2;"
                       "  border: 1px solid %3;"
                       "  border-radius: 6px;"
                       "  padding: 4px;"
                       "}"
                       "QMenu::item {"
                       "  padding: 4px 20px;"
                       "  border-radius: 4px;"
                       "}"
                       "QMenu::item:selected {"
                       "  background-color: %4;"
                       "  color: white;"
                       "}"
                       "QMenu::item:disabled {"
                       "  color: rgba(128, 128, 128, 150);"
                       "}"
                       "QMenu::separator {"
                       "  height: 1px;"
                       "  background-color: %3;"
                       "  margin: 4px 6px;"
                       "}")
            .arg(menuBg.name(), menuText.name(), menuBorder.name(),
                menuAccent.name()));
    menu.addAction(tr("New Folder"), this, &FileBrowserDialog::createFolder_);
    menu.addSeparator();
    QAction* renameAction = menu.addAction(tr("Rename"),
                                           this,
                                           &FileBrowserDialog::renameSelected_);
    renameAction->setShortcut(QKeySequence(Qt::Key_F2));
    renameAction->setEnabled(rows.size() == 1);
    QAction* deleteAction = menu.addAction(tr("Delete"),
                                           this,
                                           &FileBrowserDialog::deleteSelected_);
    deleteAction->setShortcut(QKeySequence::Delete);
    deleteAction->setEnabled(!rows.isEmpty());
    menu.exec(tree_->viewport()->mapToGlobal(pos));
}

void FileBrowserDialog::onDoubleClicked_(const QModelIndex& index)
{
    if (!index.isValid())
        return;
    const QString path = model_->filePath(index);
    if (model_->isDir(index)) {
        setDirectory_(path);
        return;
    }
    if (mode_ == Mode::OpenFile || mode_ == Mode::OpenFiles) {
        selected_ = {path};
        accept();
    }
}

void FileBrowserDialog::onSelectionChanged_()
{
    if (mode_ != Mode::Save || !nameEdit_)
        return;
    const auto rows = tree_->selectionModel()->selectedRows();
    if (rows.size() == 1 && !model_->isDir(rows.first()))
        nameEdit_->setText(model_->fileName(rows.first()));
}

void FileBrowserDialog::onFilterChanged_(int index)
{
    if (index < 0 || index >= filters_.size())
        return;
    model_->setNameFilters(filters_[index].patterns);
}

void FileBrowserDialog::tryAccept_()
{
    const auto rows = tree_->selectionModel()->selectedRows();

    switch (mode_) {
    case Mode::OpenFile: {
        for (const QModelIndex& idx : rows) {
            if (!model_->isDir(idx)) {
                selected_ = {model_->filePath(idx)};
                accept();
                return;
            }
        }
        break;
    }
    case Mode::OpenFiles: {
        QStringList files;
        for (const QModelIndex& idx : rows) {
            if (!model_->isDir(idx))
                files << model_->filePath(idx);
        }
        if (!files.isEmpty()) {
            selected_ = files;
            accept();
        }
        break;
    }
    case Mode::Save: {
        const QString name = nameEdit_->text().trimmed();
        if (name.isEmpty())
            return;
        QString fullPath = QDir(currentDir_).filePath(name);
        const int filterIdx = filterCombo_ ? filterCombo_->currentIndex() : 0;
        if (QFileInfo(fullPath).suffix().isEmpty() && filterIdx >= 0
            && filterIdx < filters_.size()
            && !filters_[filterIdx].primaryExt.isEmpty()) {
            fullPath += QLatin1Char('.') + filters_[filterIdx].primaryExt;
        }
        if (QFileInfo::exists(fullPath)) {
            const auto reply
                = showMessageBox(QMessageBox::Question,
                                 this,
                                 tr("Overwrite file?"),
                                 tr("%1 already exists. Overwrite it?")
                                     .arg(QFileInfo(fullPath).fileName()),
                                 QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::No);
            if (reply != QMessageBox::Yes)
                return;
        }
        selected_ = {fullPath};
        accept();
        break;
    }
    case Mode::SelectFolder: {
        QString path = currentDir_;
        for (const QModelIndex& idx : rows) {
            if (model_->isDir(idx)) {
                path = model_->filePath(idx);
                break;
            }
        }
        selected_ = {path};
        accept();
        break;
    }
    }
}

void FileBrowserDialog::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && windowHandle()) {
        windowHandle()->startSystemMove();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void FileBrowserDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    familiar::dialog_style::applyRoundedMask(this, 10);
}

// ============================================================================
// Free-function convenience wrappers
// ============================================================================
QString showOpenFileDialog(QWidget* parent,
                           const QString& title,
                           const QString& startDir,
                           const QString& nameFilter)
{
    FileBrowserDialog dlg(parent,
                          FileBrowserDialog::Mode::OpenFile,
                          title,
                          startDir,
                          nameFilter);
    if (dlg.exec() != QDialog::Accepted || dlg.selectedFiles().isEmpty())
        return QString();
    return dlg.selectedFiles().first();
}

QStringList showOpenFilesDialog(QWidget* parent,
                                const QString& title,
                                const QString& startDir,
                                const QString& nameFilter)
{
    FileBrowserDialog dlg(parent,
                          FileBrowserDialog::Mode::OpenFiles,
                          title,
                          startDir,
                          nameFilter);
    if (dlg.exec() != QDialog::Accepted)
        return {};
    return dlg.selectedFiles();
}

QString showSaveFileDialog(QWidget* parent,
                           const QString& title,
                           const QString& startDir,
                           const QString& nameFilter,
                           const QString& defaultFileName)
{
    FileBrowserDialog dlg(parent,
                          FileBrowserDialog::Mode::Save,
                          title,
                          startDir,
                          nameFilter,
                          defaultFileName);
    if (dlg.exec() != QDialog::Accepted || dlg.selectedFiles().isEmpty())
        return QString();
    return dlg.selectedFiles().first();
}

QString showSelectFolderDialog(QWidget* parent,
                               const QString& title,
                               const QString& startDir)
{
    FileBrowserDialog dlg(parent,
                          FileBrowserDialog::Mode::SelectFolder,
                          title,
                          startDir);
    if (dlg.exec() != QDialog::Accepted || dlg.selectedFiles().isEmpty())
        return QString();
    return dlg.selectedFiles().first();
}
