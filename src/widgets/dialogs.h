#pragma once

#include <QApplication>
#include <QBrush>
#include <QClipboard>
#include <QColor>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsItem>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLoggingCategory>
#include <QMap>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSize>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QVariant>
#include <QVariantMap>
#include <QWindow>

#include "commands.h"
#include "file_actions.h"
#include "fileio.h"
#include "log/log.h"
#include "message_box.h"
#include "recovery.h"
#include <core/settingshandler.h>
#include <widgets/dialog_style.h>

class ProgressDialog : public QProgressDialog
{
    Q_OBJECT

public:
    explicit ProgressDialog(const QString& label,
                            ThreadedIO* worker,
                            int maximum = 0,
                            QWidget* parent = nullptr)
        : QProgressDialog(label, QStringLiteral("Cancel"), 0, maximum, parent)
    {
        FLOG_DEBUG(familiar::log::Ch::UI, "Initialized progress bar");
        // See FileActions::openFile(): MainWindow's translucent/frameless
        // stylesheet cascades into this otherwise-unstyled top-level
        // dialog, painting it solid black.
        setAttribute(Qt::WA_TranslucentBackground, false);
        setStyleSheet("* { background-color: palette(window); color: "
                      "palette(window-text); }");
        setMinimumDuration(0);
        setWindowModality(Qt::WindowModal);
        setAutoReset(false);
        setAutoClose(false);
        connect(worker,
                &ThreadedIO::beginProcessing,
                this,
                &ProgressDialog::on_begin_processing);
        connect(worker,
                &ThreadedIO::progress,
                this,
                &ProgressDialog::on_progress);
        connect(worker,
                &ThreadedIO::finished,
                this,
                &ProgressDialog::on_finished);
        connect(worker,
                &ThreadedIO::userInputRequired,
                this,
                [this](const QString&) { on_finished(QString(), {}); });
        connect(this,
                &ProgressDialog::canceled,
                worker,
                &ThreadedIO::onCanceled);
    }

private slots:
    void on_progress(int value)
    {
        FLOG_DEBUG(familiar::log::Ch::UI, "Progress dialog: {}", value);
        setValue(value);
    }

    void on_begin_processing(int value)
    {
        FLOG_DEBUG(familiar::log::Ch::UI, "Begin progress dialog: {}", value);
        setMaximum(value);
    }

    void on_finished(const QString& filename, const QStringList& errors)
    {
        FLOG_DEBUG(familiar::log::Ch::UI, "Finished progress dialog");
        setValue(maximum());
        reset();
        hide();
        QTimer::singleShot(100, this, &QObject::deleteLater);
    }
};


// Live-tails familiar::log's RingSink (last N formatted lines, kept in
// memory by the logger itself - see log/ring_sink.h) rather than reading
// the log file off disk: a one-shot disk read would go stale the moment
// new lines are logged, and would show nothing at all for lines still
// sitting in quill's backend queue/buffer that haven't been flushed to
// disk yet.
class DebugLogDialog : public QDialog
{
    Q_OBJECT

public:
    DebugLogDialog(QWidget* parent)
        : QDialog(parent)
    {
        // See ChangeOpacityDialog: shown non-modally via show() below and
        // never explicitly deleted by whoever calls "new DebugLogDialog(...)".
        setAttribute(Qt::WA_DeleteOnClose);
        // See FileActions::openFile(): MainWindow's translucent/frameless
        // stylesheet cascades into this otherwise-unstyled top-level
        // dialog, painting it solid black.
        setAttribute(Qt::WA_TranslucentBackground, false);
        setStyleSheet("* { background-color: palette(window); color: "
                      "palette(window-text); }");
        setWindowTitle(qApp->applicationName() + " Debug Log");
        const QString logPath = familiar::log::logFilePath();

        log = new QPlainTextEdit();
        log->setReadOnly(true);
        log->setLineWrapMode(QPlainTextEdit::NoWrap);
        if (familiar::log::RingSink* ring = familiar::log::ringSink()) {
            log->setPlainText(ring->entries().join('\n'));
            connect(ring,
                    &familiar::log::RingSink::entryAdded,
                    this,
                    &DebugLogDialog::appendLine);
        }
        // Follow the tail as new lines come in, unless the user has
        // scrolled up to read something older.
        QScrollBar* scrollBar = log->verticalScrollBar();
        connect(scrollBar, &QScrollBar::rangeChanged, this, [this, scrollBar] {
            if (followTail_) {
                scrollBar->setValue(scrollBar->maximum());
            }
        });
        connect(scrollBar, &QScrollBar::valueChanged, this, [this, scrollBar] {
            followTail_ = scrollBar->value() == scrollBar->maximum();
        });

        QDialogButtonBox* buttons = new QDialogButtonBox(
            QDialogButtonBox::Close);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        copyButton = new QPushButton("Co&py To Clipboard");
        buttons->addButton(copyButton, QDialogButtonBox::ActionRole);
        connect(copyButton,
                &QPushButton::released,
                this,
                &DebugLogDialog::copyToClipboard);

        QVBoxLayout* layout = new QVBoxLayout();
        setLayout(layout);
        QLabel* nameWidget = new QLabel(logPath);
        nameWidget->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(nameWidget);
        layout->addWidget(log);
        layout->addWidget(buttons);
        show();
    }

private:
    QPlainTextEdit* log;
    QPushButton* copyButton;
    bool followTail_ = true;

private slots:
    void appendLine(const QString& line) { log->appendPlainText(line); }

    void copyToClipboard()
    {
        QClipboard* clipboard = QApplication::clipboard();
        clipboard->setText(log->toPlainText());
    }
};


// Crash-recovery prompt - shown once at startup if
// familiar::recovery::scan() finds leftover snapshots from a session
// that didn't exit cleanly. Each entry gets its own checkbox, which
// decides its fate when Restore is clicked: checked entries are opened
// in a fresh tab (FileActions::restoreFromRecovery()), unchecked ones
// are discarded - there's no third "leave it as-is" state, so a
// checkbox is a real yes/no decision, not just a restore-shortlist.
// Discard (with a confirmation, since it's irreversible) wipes
// everything regardless of check state, for "none of this, thanks".
// Closing the dialog without clicking either (X, Escape) is the actual
// "decide later" path - nothing is touched, so scan() finds the same
// entries again next launch.
class RecoveryDialog : public QDialog
{
    Q_OBJECT

public:
    RecoveryDialog(FileActions& fileActions,
                   const QList<familiar::recovery::Entry>& entries,
                   QWidget* parent)
        : QDialog(parent)
        , fileActions_(fileActions)
        , entries_(entries)
    {
        // See ChangeOpacityDialog: shown non-modally via show() below and
        // never explicitly deleted by whoever calls "new RecoveryDialog(...)".
        setAttribute(Qt::WA_DeleteOnClose);
        // See FileActions::openFile(): MainWindow's translucent/frameless
        // stylesheet cascades into this otherwise-unstyled top-level
        // dialog, painting it solid black.
        setAttribute(Qt::WA_TranslucentBackground, false);
        setStyleSheet("* { background-color: palette(window); color: "
                      "palette(window-text); }");
        setWindowTitle(tr("Autosave Recovery"));

        auto* layout = new QVBoxLayout(this);
        auto* label = new QLabel(
            tr("The following files can be recovered from the autosave "
               "folder. Uncheck any you don't want, then Restore - or "
               "Discard all of them."));
        label->setWordWrap(true);
        layout->addWidget(label);

        list_ = new QListWidget(this);
        for (const familiar::recovery::Entry& e : entries_) {
            auto* item = new QListWidgetItem(e.label, list_);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Checked);
        }
        layout->addWidget(list_);

        auto* buttonsRow = new QHBoxLayout();
        auto* restoreBtn = new QPushButton(tr("Restore"));
        auto* discardBtn = new QPushButton(tr("Discard"));
        buttonsRow->addWidget(restoreBtn);
        buttonsRow->addWidget(discardBtn);
        buttonsRow->addStretch();
        layout->addLayout(buttonsRow);

        connect(restoreBtn,
                &QPushButton::clicked,
                this,
                &RecoveryDialog::restoreAndDiscardUnchecked);
        connect(discardBtn,
                &QPushButton::clicked,
                this,
                &RecoveryDialog::discardAll);

        show();
    }

private:
    FileActions& fileActions_;
    QList<familiar::recovery::Entry> entries_;
    QListWidget* list_ = nullptr;

private slots:
    void restoreAndDiscardUnchecked()
    {
        // Captured BEFORE any restore starts - see FileActions::
        // closeTab()'s comment for why this can't just be re-found by
        // scanning "untitled && unmodified" again after the fact.
        CanvasView* blankTab = fileActions_.findBlankTab();

        bool restoredAny = false;
        for (int i = 0; i < list_->count(); ++i) {
            const familiar::recovery::Entry& e = entries_[i];
            if (list_->item(i)->checkState() == Qt::Checked) {
                // restoreFromRecovery() loads in the background and
                // deletes this entry's recovery file itself, only once
                // that load actually succeeds - NOT deleted here, since
                // the load hasn't necessarily opened the file yet by the
                // time this call returns (see FileActions::
                // loadFmlIntoCurrentTab()).
                fileActions_.restoreFromRecovery(e.fmlPath,
                                                 e.originalPath,
                                                 e.id);
                restoredAny = true;
            } else {
                // Unchecked is a real "no" here, not "leave for later" -
                // see the class comment above.
                familiar::recovery::remove(e.id);
            }
        }
        // Only if something was actually restored - otherwise this is
        // the only tab left open, and closing it would leave zero.
        if (restoredAny && blankTab) {
            fileActions_.closeTab(blankTab);
        }
        close();
    }

    void discardAll()
    {
        const auto reply = showMessageBox(
            QMessageBox::Question,
            this,
            tr("Discard all recoverable files?"),
            tr("This deletes all %1 recoverable file(s) permanently, "
               "regardless of their checkboxes. This can't be undone.")
                .arg(entries_.size()),
            QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Cancel);
        if (reply != QMessageBox::Discard) {
            return;
        }
        for (const familiar::recovery::Entry& e : entries_) {
            familiar::recovery::remove(e.id);
        }
        close();
    }
};


class SceneToPixmapExporterDialog : public QDialog
{
    Q_OBJECT

    static constexpr int MIN_SIZE = 10;
    static constexpr int MAX_SIZE = 100000;

public:
    SceneToPixmapExporterDialog(QWidget* parent, QSize defaultSize)
        : QDialog(parent)
        , defaultSize(defaultSize)
        , ignoreChange(false)
    {
        if (defaultSize.width() > MAX_SIZE || defaultSize.width() >= MAX_SIZE) {
            defaultSize.scale(MAX_SIZE, MAX_SIZE, Qt::KeepAspectRatio);
        }

        // See FileActions::openFile(): MainWindow's translucent/frameless
        // stylesheet cascades into this otherwise-unstyled top-level
        // dialog, painting it solid black.
        setAttribute(Qt::WA_TranslucentBackground, false);
        setStyleSheet("* { background-color: palette(window); color: "
                      "palette(window-text); }");
        setWindowTitle("Export Scene to Image");
        setWindowModality(Qt::WindowModal);
        QGridLayout* layout = new QGridLayout();
        setLayout(layout);

        layout->addWidget(new QLabel("Width:"), 0, 0);
        widthInput = new QSpinBox();
        widthInput->setRange(MIN_SIZE, MAX_SIZE);
        widthInput->setValue(defaultSize.width());
        connect(widthInput,
                &QSpinBox::valueChanged,
                this,
                &SceneToPixmapExporterDialog::onWidthChanged);
        layout->addWidget(widthInput, 0, 1);

        layout->addWidget(new QLabel("Height:"), 1, 0);
        heightInput = new QSpinBox();
        heightInput->setRange(MIN_SIZE, MAX_SIZE);
        heightInput->setValue(defaultSize.height());
        connect(heightInput,
                &QSpinBox::valueChanged,
                this,
                &SceneToPixmapExporterDialog::onHeightChanged);
        layout->addWidget(heightInput, 1, 1);

        QDialogButtonBox* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons, 3, 1);
    }

    QSize value() const
    {
        return QSize(widthInput->value(), heightInput->value());
    }

private slots:
    void onWidthChanged(int width)
    {
        if (!ignoreChange) {
            ignoreChange = true;
            QSize scaled = defaultSize.scaled(width,
                                              MAX_SIZE,
                                              Qt::KeepAspectRatio);
            heightInput->setValue(scaled.height());
            ignoreChange = false;
        }
    }

    void onHeightChanged(int height)
    {
        if (!ignoreChange) {
            ignoreChange = true;
            QSize scaled = defaultSize.scaled(MAX_SIZE,
                                              height,
                                              Qt::KeepAspectRatio);
            widthInput->setValue(scaled.width());
            ignoreChange = false;
        }
    }

private:
    QSize defaultSize;
    bool ignoreChange;
    QSpinBox* widthInput;
    QSpinBox* heightInput;
};


// Custom-chrome convention (see ExportImagesFileExistsDialog/
// ColorPickerDialog): frameless, opaque (NOT WA_TranslucentBackground -
// see dialog_style::panelStyleSheet()'s own comment), rounded via a real
// setMask() rather than QSS border-radius alone, drag-to-move via
// startSystemMove() since there's no native title bar to grab.
class ChangeOpacityDialog : public QDialog
{
    Q_OBJECT

public:
    ChangeOpacityDialog(QWidget* parent,
                        const QList<QGraphicsItem*>& items,
                        QUndoStack* undoStack)
        : QDialog(parent)
        , items(items)
        , undoStack(undoStack)
        , command(new ChangeOpacityCommand(items, 1.0))
    {
        int value = !items.isEmpty() ? int(items[0]->opacity() * 100) : 100;

        setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground, false);
        setAttribute(Qt::WA_StyledBackground);

        // Not owned/deleted anywhere by the caller (it's a "new
        // ChangeOpacityDialog(...)" fire-and-forget, WindowModal rather
        // than exec()'d) - without this it'd just stay a hidden child of
        // its parent, accumulating for as long as that parent's alive,
        // on every single open. command is already nulled out by both
        // accept() and reject() before this fires, so the destructor's
        // "delete command" is a safe no-op either way.
        setAttribute(Qt::WA_DeleteOnClose);
        setWindowTitle(tr("Change Opacity"));
        setWindowModality(Qt::WindowModal);
        setFixedWidth(280);

        auto colorPreset
            = SettingsHandler::getInstance()->getCurrentColorPreset();
        const QColor& textColor = colorPreset[EPresetsColorIdx::kTextColor];
        const QColor& background
            = colorPreset[EPresetsColorIdx::kBackgroundColor];
        const QColor& border = colorPreset[EPresetsColorIdx::kBorderColor];
        const QColor& accent = colorPreset[EPresetsColorIdx::kSelectionColor];

        auto* shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(24);
        shadow->setOffset(0, 4);
        shadow->setColor(QColor(0, 0, 0, 150));
        setGraphicsEffect(shadow);

        auto* outer = new QVBoxLayout(this);
        outer->setContentsMargins(20, 14, 20, 16);
        outer->setSpacing(14);

        auto* topRow = new QHBoxLayout();
        auto* titleLabel = new QLabel(tr("Change Opacity"), this);
        QFont titleFont = titleLabel->font();
        titleFont.setBold(true);
        titleFont.setPointSize(titleFont.pointSize() + 1);
        titleLabel->setFont(titleFont);
        topRow->addWidget(titleLabel, 1);

        auto* closeBtn = new QPushButton(QStringLiteral("×"), this);
        closeBtn->setFixedSize(22, 22);
        closeBtn->setCursor(Qt::PointingHandCursor);
        closeBtn->setFocusPolicy(Qt::NoFocus);
        closeBtn->setObjectName(QStringLiteral("changeOpacityCloseBtn"));
        connect(closeBtn,
                &QPushButton::clicked,
                this,
                &ChangeOpacityDialog::reject);
        topRow->addWidget(closeBtn);
        outer->addLayout(topRow);

        label = new QLabel(tr("Opacity: %1%").arg(value), this);
        outer->addWidget(label);

        input = new QSlider(Qt::Horizontal, this);
        input->setObjectName(QStringLiteral("changeOpacitySlider"));
        input->setRange(0, 100);
        connect(input,
                &QSlider::valueChanged,
                this,
                &ChangeOpacityDialog::onValueChanged);
        input->setValue(value);
        outer->addWidget(input);

        auto* buttonRow = new QHBoxLayout();
        buttonRow->addStretch();
        auto* cancelBtn = new QPushButton(tr("Cancel"), this);
        familiar::dialog_style::styleSecondaryButton(cancelBtn,
                                                     textColor,
                                                     border);
        connect(cancelBtn,
                &QPushButton::clicked,
                this,
                &ChangeOpacityDialog::reject);
        buttonRow->addWidget(cancelBtn);
        auto* okBtn = new QPushButton(tr("OK"), this);
        familiar::dialog_style::stylePrimaryButton(okBtn, accent);
        okBtn->setDefault(true);
        connect(okBtn,
                &QPushButton::clicked,
                this,
                &ChangeOpacityDialog::accept);
        buttonRow->addWidget(okBtn);
        outer->addLayout(buttonRow);

        setStyleSheet(
            familiar::dialog_style::panelStyleSheet("ChangeOpacityDialog",
                                                    background,
                                                    border,
                                                    textColor)
            + familiar::dialog_style::closeButtonStyleSheet(
                "changeOpacityCloseBtn", textColor, accent)
            // Explicit ::groove/::handle rules, not just a bare QSlider
            // color rule - same reasoning as QRadioButton's ::indicator
            // in ExportImagesFileExistsDialog: QSS with no sub-control
            // rule kills the native rendering rather than just recoloring
            // it.
            + QStringLiteral("QSlider::groove:horizontal {"
                             "  height: 4px;"
                             "  background: %1;"
                             "  border-radius: 2px;"
                             "}"
                             "QSlider::sub-page:horizontal {"
                             "  height: 4px;"
                             "  background: %2;"
                             "  border-radius: 2px;"
                             "}"
                             "QSlider::handle:horizontal {"
                             "  width: 14px;"
                             "  height: 14px;"
                             "  margin: -5px 0;"
                             "  border-radius: 7px;"
                             "  background: %2;"
                             "  border: 1px solid %2;"
                             "}")
                  .arg(border.name(), accent.name()));

        show();
    }

    ~ChangeOpacityDialog() { delete command; }

public slots:
    void accept() override
    {
        if (!items.isEmpty()) {
            command->setIgnoreFirstRedo(true);
            undoStack->push(command);
            command = nullptr;
        }
        QDialog::accept();
    }

    void reject() override
    {
        if (command) {
            command->undo();
            delete command;
            command = nullptr;
        }
        QDialog::reject();
    }

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && windowHandle()) {
            windowHandle()->startSystemMove();
            event->accept();
            return;
        }
        QDialog::mousePressEvent(event);
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QDialog::resizeEvent(event);
        familiar::dialog_style::applyRoundedMask(this, 10);
    }

private slots:
    void onValueChanged(int value)
    {
        label->setText(tr("Opacity: %1%").arg(value));
        command->setOpacity(value / 100.0);
        command->redo();
    }

private:
    QList<QGraphicsItem*> items;
    QUndoStack* undoStack;
    ChangeOpacityCommand* command;
    QLabel* label;
    QSlider* input;
};


class FamNotification : public QWidget
{
    Q_OBJECT

public:
    FamNotification(QWidget* parent, const QString& text)
        : QWidget(parent)
    {
        QLabel* label = new QLabel(text);
        setObjectName("FamNotification");
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAutoFillBackground(true);
        QVBoxLayout* layout = new QVBoxLayout();
        layout->addWidget(label);
        setLayout(layout);

        QColor color = QApplication::palette().color(QPalette::Window);
        setStyleSheet(QString("background-color: rgba(%1, %2, %3, 0.9); "
                              "padding: 0.7em; border-radius: 5px;")
                          .arg(color.red())
                          .arg(color.green())
                          .arg(color.blue()));

        show();
        int x = (parent->width() - width()) / 2;
        move(x, 10);

        QTimer::singleShot(1000 * 3, this, &QObject::deleteLater);
    }
};


class SampleColorWidget : public QWidget
{
    Q_OBJECT

    static constexpr int OFFSET = 10;
    static constexpr int SIZE = 50;

public:
    SampleColorWidget(QWidget* parent,
                      const QPointF& pos,
                      const QColor& color = QColor())
        : QWidget(parent)
        , m_color(color)
    {
        setFixedSize(SIZE, SIZE);
        setPos(pos);
        show();
    }

    void setPos(const QPointF& pos)
    {
        move(int(pos.x() + OFFSET), int(pos.y() + OFFSET));
    }

    void update(const QPointF& pos, const QColor& color)
    {
        setPos(pos);
        m_color = color;
        repaint();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QColor color = m_color.isValid() ? m_color : QColor(0, 0, 0, 0);
        QPainter painter(this);
        painter.setBrush(QBrush(color));
        painter.setPen(Qt::NoPen);
        painter.drawRect(0, 0, SIZE, SIZE);
    }

private:
    QColor m_color;
};


// Custom-chrome convention (see FileBrowserDialog/HelpDialog/RenameDialog
// (ui/hierarchy_panel.cpp)): frameless, opaque (NOT WA_TranslucentBackground
// - see dialog_style::panelStyleSheet()'s own comment for why that combo
// breaks rendering on a QSS-auto-painted top-level widget), rounded via
// a real setMask() rather than QSS border-radius alone, drag-to-move via
// startSystemMove() since there's no native title bar to grab.
class ExportImagesFileExistsDialog : public QDialog
{
    Q_OBJECT

public:
    ExportImagesFileExistsDialog(QWidget* parent, const QString& filename)
        : QDialog(parent)
    {
        setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground, false);
        setAttribute(Qt::WA_StyledBackground);
        setWindowModality(Qt::ApplicationModal);
        setFixedWidth(380);

        auto colorPreset
            = SettingsHandler::getInstance()->getCurrentColorPreset();
        const QColor& textColor = colorPreset[EPresetsColorIdx::kTextColor];
        const QColor& background
            = colorPreset[EPresetsColorIdx::kBackgroundColor];
        const QColor& border = colorPreset[EPresetsColorIdx::kBorderColor];
        const QColor& accent = colorPreset[EPresetsColorIdx::kSelectionColor];

        auto* shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(24);
        shadow->setOffset(0, 4);
        shadow->setColor(QColor(0, 0, 0, 150));
        setGraphicsEffect(shadow);

        auto* outer = new QVBoxLayout(this);
        outer->setContentsMargins(20, 14, 20, 16);
        outer->setSpacing(12);

        auto* topRow = new QHBoxLayout();
        auto* titleLabel = new QLabel(tr("File Exists"), this);
        QFont titleFont = titleLabel->font();
        titleFont.setBold(true);
        titleFont.setPointSize(titleFont.pointSize() + 1);
        titleLabel->setFont(titleFont);
        topRow->addWidget(titleLabel, 1);

        auto* closeBtn = new QPushButton(QStringLiteral("×"), this);
        closeBtn->setFixedSize(22, 22);
        closeBtn->setCursor(Qt::PointingHandCursor);
        closeBtn->setFocusPolicy(Qt::NoFocus);
        closeBtn->setObjectName(QStringLiteral("exportExistsCloseBtn"));
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
        topRow->addWidget(closeBtn);
        outer->addLayout(topRow);

        auto* messageLabel
            = new QLabel(tr("This file already exists:\n%1").arg(filename),
                         this);
        messageLabel->setWordWrap(true);
        outer->addWidget(messageLabel);

        const QList<QPair<QString, QString>> choices = {
            {QStringLiteral("skip"), tr("Skip this file")},
            {QStringLiteral("skip_all"), tr("Skip all existing files")},
            {QStringLiteral("overwrite"), tr("Overwrite this file")},
            {QStringLiteral("overwrite_all"),
             tr("Overwrite all existing files")},
        };
        for (const auto& [value, label] : choices) {
            auto* btn = new QRadioButton(label, this);
            radioButtons.insert(value, btn);
            outer->addWidget(btn);
        }
        radioButtons[QStringLiteral("skip")]->setChecked(true);

        auto* buttonRow = new QHBoxLayout();
        buttonRow->addStretch();
        auto* cancelBtn = new QPushButton(tr("Cancel"), this);
        familiar::dialog_style::styleSecondaryButton(cancelBtn,
                                                     textColor,
                                                     border);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        buttonRow->addWidget(cancelBtn);
        auto* okBtn = new QPushButton(tr("Continue"), this);
        familiar::dialog_style::stylePrimaryButton(okBtn, accent);
        okBtn->setDefault(true);
        connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
        buttonRow->addWidget(okBtn);
        outer->addLayout(buttonRow);

        setStyleSheet(
            familiar::dialog_style::panelStyleSheet(
                "ExportImagesFileExistsDialog", background, border, textColor)
            + familiar::dialog_style::closeButtonStyleSheet(
                "exportExistsCloseBtn", textColor, accent)
            // Explicit ::indicator rules, not just a bare QRadioButton
            // color rule - QSS on a QRadioButton/QCheckBox with no
            // ::indicator rule of its own kills the native indicator
            // rendering entirely (confirmed the hard way on QCheckBox
            // during step 24's dialog pass - same underlying Qt/QSS
            // behavior applies here).
            + QStringLiteral(
                  "QRadioButton { color: %1; padding: 2px 0; }"
                  "QRadioButton::indicator { width: 14px; height: 14px; }"
                  "QRadioButton::indicator:unchecked {"
                  "  border: 1px solid %2;"
                  "  border-radius: 7px;"
                  "  background-color: rgba(0, 0, 0, 20);"
                  "}"
                  "QRadioButton::indicator:checked {"
                  "  border: 1px solid %3;"
                  "  border-radius: 7px;"
                  "  background-color: %3;"
                  "}")
                  .arg(textColor.name(), border.name(), accent.name()));
    }

    QString getAnswer() const
    {
        for (auto it = radioButtons.constBegin(); it != radioButtons.constEnd();
             ++it) {
            if (it.value()->isChecked()) {
                return it.key();
            }
        }
        return QStringLiteral("skip");
    }

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && windowHandle()) {
            windowHandle()->startSystemMove();
            event->accept();
            return;
        }
        QDialog::mousePressEvent(event);
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QDialog::resizeEvent(event);
        familiar::dialog_style::applyRoundedMask(this, 10);
    }

private:
    QMap<QString, QRadioButton*> radioButtons;
};
