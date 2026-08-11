#include "file_browser_dialog.h"
#include "dialog_style.h"
#include "message_box.h"

#include <core/settingshandler.h>

#include <QComboBox>
#include <QDir>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFont>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
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
                        "}")
                        .arg(textColor.name(), border.name(), accent.name()));

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
