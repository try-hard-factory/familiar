#pragma once

#include <QApplication>
#include <QBrush>
#include <QCheckBox>
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
#include <QPointer>
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
#include <widgets/flat_checkbox.h>
#include <widgets/flat_progress_bar.h>

// Custom-chrome convention (see ChangeOpacityDialog/RawImportDialog/
// ExportImagesFileExistsDialog below): frameless, opaque (NOT
// WA_TranslucentBackground - see dialog_style::panelStyleSheet()'s own
// comment for why that combo breaks rendering on a QSS-auto-painted
// top-level widget), rounded via a real setMask() rather than QSS
// border-radius alone, drag-to-move via startSystemMove() since there's
// no native title bar to grab.
//
// Was a QProgressDialog. Swapped to a plain QDialog for the same reason
// every other stock Qt dialog in this app got replaced during the
// custom-widgets pass: its fixed internal layout (label + bar + button,
// all native-styled) can't carry this app's own chrome, and it left no
// room for the "which file is being worked on right now" line. What it
// actually provided beyond that was small and is reimplemented below -
// setValue()/setRange()/maximum()/reset() over an owned FlatProgressBar,
// plus a canceled() signal - so all five construction sites
// (canvasview.cpp x3, file_actions.cpp, plus the reusable import one)
// keep working unchanged.
//
// NOTE the ctor calls show() itself: QProgressDialog used to self-show
// via setMinimumDuration(0), a plain QDialog does not, and every caller
// here is "new ProgressDialog(...)" with no show() of its own.
class ProgressDialog : public QDialog
{
    Q_OBJECT

    static constexpr int kDialogWidth = 380;
    // Usable width inside outer's own left/right margins below - what
    // the "current item" line elides against. A constant rather than the
    // label's live width() so eliding is deterministic even before the
    // dialog has ever been laid out.
    static constexpr int kContentWidth = kDialogWidth - 40;

public:
    explicit ProgressDialog(const QString& label,
                            ThreadedIO* worker,
                            int maximum = 0,
                            QWidget* parent = nullptr)
        : QDialog(parent)
    {
        setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground, false);
        setAttribute(Qt::WA_StyledBackground);
        setWindowModality(Qt::WindowModal);
        setFixedWidth(kDialogWidth);

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
        outer->setSpacing(10);

        auto* topRow = new QHBoxLayout();
        auto* titleLabel = new QLabel(label, this);
        QFont titleFont = titleLabel->font();
        titleFont.setBold(true);
        titleFont.setPointSize(titleFont.pointSize() + 1);
        titleLabel->setFont(titleFont);
        topRow->addWidget(titleLabel, 1);

        auto* closeBtn = new QPushButton(QStringLiteral("×"), this);
        closeBtn->setFixedSize(22, 22);
        closeBtn->setCursor(Qt::PointingHandCursor);
        closeBtn->setFocusPolicy(Qt::NoFocus);
        closeBtn->setObjectName(QStringLiteral("progressCloseBtn"));
        // Same funnel as the Cancel button below - see reject().
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
        topRow->addWidget(closeBtn, 0, Qt::AlignTop);
        outer->addLayout(topRow);

        // The whole point of the rewrite: which item is being worked on
        // right now (ThreadedIO::currentItemChanged, fileio.h). Kept at a
        // fixed height even while empty so the dialog doesn't jump around
        // as names come and go.
        currentItemLabel_ = new QLabel(this);
        // Muted via an objectName + its own QSS rule further down, NOT
        // via setPalette(): dialog_style::panelStyleSheet() below carries
        // a blanket "QLabel { color: ... }" rule, and QSS beats QPalette
        // - a palette-based tint here would simply never show up.
        currentItemLabel_->setObjectName(
            QStringLiteral("progressCurrentItem"));
        currentItemLabel_->setFixedHeight(
            currentItemLabel_->fontMetrics().height());
        outer->addWidget(currentItemLabel_);

        auto* barRow = new QHBoxLayout();
        barRow->setSpacing(10);
        bar_ = new FlatProgressBar(border, accent, this);
        barRow->addWidget(bar_, 1);
        percentLabel_ = new QLabel(this);
        // Reserves the widest string this ever shows, so the bar beside
        // it doesn't resize by a few pixels every time the number's digit
        // count changes.
        percentLabel_->setFixedWidth(
            percentLabel_->fontMetrics().horizontalAdvance(
                QStringLiteral("100%")));
        percentLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        barRow->addWidget(percentLabel_, 0);
        outer->addLayout(barRow);

        auto* buttonRow = new QHBoxLayout();
        buttonRow->addStretch();
        auto* cancelBtn = new QPushButton(tr("Cancel"), this);
        familiar::dialog_style::styleSecondaryButton(cancelBtn,
                                                     textColor,
                                                     border);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        buttonRow->addWidget(cancelBtn);
        outer->addLayout(buttonRow);

        setStyleSheet(
            familiar::dialog_style::panelStyleSheet("ProgressDialog",
                                                    background,
                                                    border,
                                                    textColor)
            + familiar::dialog_style::closeButtonStyleSheet(
                "progressCloseBtn", textColor, accent)
            // Overrides panelStyleSheet()'s blanket QLabel colour for
            // this one label - the filename is secondary to the
            // operation title above it, so it reads as a subdued
            // subtitle rather than competing with it.
            + QStringLiteral(
                  "QLabel#progressCurrentItem { color: rgba(%1, %2, %3, 160); }")
                  .arg(textColor.red())
                  .arg(textColor.green())
                  .arg(textColor.blue()));

        knownMaximum_ = maximum;
        bar_->setRange(0, maximum);
        updatePercentText_();
        bindWorker_(worker);
        show();
    }

    // Reattaches this SAME dialog to a new import session's worker,
    // instead of the caller constructing a brand new ProgressDialog each
    // time (see canvasview.cpp's imageImportProgressDialog_ - now a
    // single long-lived instance, created once and reused for every
    // do_insert_images() call). Two real problems this replaced, in
    // order: (1) a confirmed UAF crash from destroying this dialog
    // mid-session (a crash backtrace's `this=` pointer matched an
    // address this class's own destructor had already logged as
    // destroyed) - fixed by simply never destroying it mid-session; (2)
    // that fix's own real problem, correctly called out by Max: never
    // destroying it but still constructing a NEW one per import left
    // every previous one alive-but-hidden forever (as a child of
    // CanvasView), i.e. N separate imports over a session == N
    // permanently leaked dialogs until the tab itself closed. This
    // rebind() is the actual fix for both at once: one object, reused,
    // memory bounded regardless of how many imports happen.
    void rebind(ThreadedIO* worker)
    {
        FLOG_DEBUG(familiar::log::Ch::UI,
                  "ProgressDialog::rebind() this={} old worker={} new "
                  "worker={}",
                  static_cast<void*>(this),
                  static_cast<void*>(worker_),
                  static_cast<void*>(worker));
        if (worker_) {
            // Both directions - see bindWorker_()'s own connect() calls
            // (worker_->this AND this->worker_, the canceled/onCanceled
            // one) - QObject::disconnect(sender, nullptr, receiver,
            // nullptr) drops every connection between exactly this pair,
            // nothing else.
            QObject::disconnect(worker_, nullptr, this, nullptr);
            QObject::disconnect(this, nullptr, worker_, nullptr);
        }
        finished_ = false;
        knownMaximum_ = 0;
        currentItemLabel_->clear();
        setRange(0, 0);
        bindWorker_(worker);
        show();
    }

    // Call once, right after construction, ONLY for a ProgressDialog the
    // caller intends to keep and rebind() across multiple operations
    // (canvasview.cpp's imageImportProgressDialog_) - every other
    // (one-shot, fire-and-forget) ProgressDialog in this app must leave
    // this false (the default) so on_finished() keeps self-deleting
    // normally. See on_finished()'s own comment for the real leak this
    // distinction fixes.
    void setReusable(bool value) { reusable_ = value; }

    // DIAG (SIGSEGV investigation): logs this object's own address right
    // as it's actually destroyed - compare its timestamp/address against
    // any later crash backtrace's `this=` pointer - this is exactly how
    // the real UAF above was confirmed (a crash's `this=` matched an
    // address logged here, already destroyed, ~800ms earlier). Now only
    // fires when CanvasView/the tab itself is destroyed (see
    // on_finished()'s own comment - no more mid-session deleteLater()),
    // still kept as a cheap correctness check.
    ~ProgressDialog() override
    {
        FLOG_DEBUG(familiar::log::Ch::UI,
                  "~ProgressDialog() this={}",
                  static_cast<void*>(this));
    }

signals:
    // QProgressDialog's own signal of the same name, reimplemented -
    // bindWorker_() wires it to ThreadedIO::onCanceled() exactly as
    // before. Single funnel: the Cancel button, the "x" button and Esc
    // all go through reject() below, which emits this.
    void canceled();

public slots:
    // Esc (QDialog's own default) as well as both buttons. Deliberately
    // NOT also hiding on its own beyond what QDialog::reject() does: the
    // worker still has to notice ThreadedIO::canceled and wind down,
    // which ends in on_finished() like any other completion.
    void reject() override
    {
        emit canceled();
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

private:
    // Shared by the constructor and rebind() above - the actual
    // connect() calls, once, instead of duplicated in both places.
    void bindWorker_(ThreadedIO* worker)
    {
        // DIAG (SIGSEGV investigation): this object's own address, to
        // correlate against a crash backtrace's `this=` pointer and
        // against ~ProgressDialog()'s own DIAG log above.
        FLOG_DEBUG(familiar::log::Ch::UI,
                  "ProgressDialog::bindWorker_() this={} worker={}",
                  static_cast<void*>(this),
                  static_cast<void*>(worker));
        worker_ = worker;
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
                &ThreadedIO::currentItemChanged,
                this,
                &ProgressDialog::on_current_item_changed);
        connect(worker,
                &ThreadedIO::userInputRequired,
                this,
                [this](const QString&) { on_finished(QString(), {}); });
        // Deliberately NOT reacting to ThreadedIO::rawImportChoiceRequired
        // the same way (see ImagesToDirectoryExporter's own userInputRequired
        // pause above) - unlike that flow, CanvasView keeps this SAME
        // ProgressDialog alive across a RAW-choice pause+resume instead
        // of treating a pause as "done".
        connect(this,
                &ProgressDialog::canceled,
                worker,
                &ThreadedIO::onCanceled);
        // While a RAW file is actively decoding, this bar temporarily
        // shows THAT FILE's own 0-100 demosaic sub-progress
        // (rawDecodeProgress below - LibRaw::set_progress_handler(),
        // fileio.cpp) instead of "item N of M" - progress() alone only
        // advances once a whole item finishes, and a full-resolution
        // demosaic can take 10+ seconds; without this the bar just sat frozen
        // the whole time. Snaps back to the real N-of-M range the
        // moment this file's decode ends (on_raw_decode_state_changed(
        // false) below, and on_progress() itself as a backstop).
        connect(worker,
                &ThreadedIO::rawDecodeStateChanged,
                this,
                &ProgressDialog::on_raw_decode_state_changed);
        connect(worker,
                &ThreadedIO::rawDecodeProgress,
                this,
                &ProgressDialog::on_raw_decode_progress);
    }

    // ─── The QProgressDialog API this class used to inherit ─────────────
    void setRange(int minimum, int maximum)
    {
        bar_->setRange(minimum, maximum);
        updatePercentText_();
    }

    void setMaximum(int maximum)
    {
        bar_->setMaximum(maximum);
        updatePercentText_();
    }

    int maximum() const { return bar_->maximum(); }

    void setValue(int value)
    {
        bar_->setValue(value);
        updatePercentText_();
    }

    void reset() { bar_->setValue(bar_->minimum()); }

    // Blank rather than "0%" while indeterminate (the marquee already
    // says "working, total unknown" on its own) or before any real range
    // exists - a hard 0% next to a sliding bar reads as stuck.
    void updatePercentText_()
    {
        const int span = bar_->maximum() - bar_->minimum();
        if (bar_->isIndeterminate() || span <= 0) {
            percentLabel_->clear();
            return;
        }
        const int percent = ((bar_->value() - bar_->minimum()) * 100) / span;
        percentLabel_->setText(QStringLiteral("%1%").arg(percent));
    }

private slots:
    void on_current_item_changed(const QString& name)
    {
        if (finished_) {
            return;
        }
        currentItemLabel_->setText(
            currentItemLabel_->fontMetrics().elidedText(name,
                                                        Qt::ElideMiddle,
                                                        kContentWidth));
    }

    void on_progress(int value)
    {
        // DIAG: logged UNCONDITIONALLY, before the guard - a prior
        // build only logged this AFTER the finished_ check, so a crash
        // during setValue() left no trace of whether the guard had even
        // been reached yet.
        FLOG_DEBUG(familiar::log::Ch::UI,
                  "on_progress({}) this={} finished_={}",
                  value,
                  static_cast<void*>(this),
                  finished_);
        if (finished_) {
            return;
        }
        // Back to the real range in case a RAW decode's own sub-progress
        // mode was still active - belt-and-suspenders alongside
        // on_raw_decode_state_changed(false) below, which normally
        // already restores it right before this fires anyway.
        setRange(0, knownMaximum_);
        setValue(value);
    }

    void on_raw_decode_state_changed(bool decoding)
    {
        FLOG_DEBUG(familiar::log::Ch::UI,
                  "on_raw_decode_state_changed({}) this={} finished_={}",
                  decoding,
                  static_cast<void*>(this),
                  finished_);
        if (finished_) {
            return;
        }
        if (decoding) {
            setRange(0, 100);
            setValue(0);
        } else {
            setRange(0, knownMaximum_);
        }
    }

    void on_raw_decode_progress(int percent)
    {
        FLOG_DEBUG(familiar::log::Ch::UI,
                  "on_raw_decode_progress({}) this={} finished_={}",
                  percent,
                  static_cast<void*>(this),
                  finished_);
        if (finished_) {
            return;
        }
        setValue(percent);
    }

    void on_begin_processing(int value)
    {
        FLOG_DEBUG(familiar::log::Ch::UI,
                  "on_begin_processing({}) this={} finished_={}",
                  value,
                  static_cast<void*>(this),
                  finished_);
        if (finished_) {
            return;
        }
        knownMaximum_ = value;
        setMaximum(value);
    }

    void on_finished(const QString& filename, const QStringList& errors)
    {
        // finished_/worker_->disconnect() below guard this OBJECT while
        // it's alive - neither helps against what turned out to be the
        // actual bug (confirmed via DIAG logging + a backtrace: the
        // crash's `this=` pointer matched an address this class's own
        // destructor had ALREADY logged as destroyed, ~800ms earlier -
        // a stale queued event reaching genuinely freed memory, not a
        // same-object re-entry). Reading `this->finished_` on freed
        // memory is undefined behavior regardless of what value happens
        // to be sitting there.
        //
        // ONLY the reusable_ instance (canvasview.cpp's
        // imageImportProgressDialog_, marked via setReusable(true) right
        // after construction - see that call site) skips destruction
        // here, staying alive/hidden, parented to CanvasView, until the
        // tab itself closes - that's the one this class was built to
        // survive a mid-session rebind() for. Real regression this
        // fixes: EVERY other ProgressDialog in this app (file_actions.cpp's
        // "Opening project", canvasview.cpp's 3 "Exporting..." dialogs) is
        // a genuine one-shot, constructed fresh and never touched again -
        // those need to keep self-deleting normally via deleteLater(),
        // or each export/open-project operation leaks one hidden dialog
        // for the rest of the app's life (confirmed for real: Max saw
        // TWO ~ProgressDialog() calls at shutdown with only ONE tab and
        // ONE RAW import all session - the second was an export/open-
        // project dialog this fix had accidentally stopped cleaning up
        // too, since the original fix touched every ProgressDialog
        // instance instead of only the reused one).
        FLOG_DEBUG(familiar::log::Ch::UI,
                  "on_finished() this={} finished_={} reusable_={}",
                  static_cast<void*>(this),
                  finished_,
                  reusable_);
        if (finished_) {
            return;
        }
        finished_ = true;
        if (worker_) {
            worker_->disconnect(this);
        }
        setRange(0, knownMaximum_); // in case a RAW decode's indeterminate
                                    // mode was somehow still active
        setValue(maximum());
        reset();
        currentItemLabel_->clear();
        hide();
        if (!reusable_) {
            QTimer::singleShot(100, this, &QObject::deleteLater);
        }
    }

private:
    FlatProgressBar* bar_ = nullptr;
    QLabel* percentLabel_ = nullptr;
    QLabel* currentItemLabel_ = nullptr;
    // setMaximum()'s own value can't be read back once setRange(0, 0)
    // (indeterminate mode, on_raw_decode_state_changed() above) is
    // active - maximum() itself reads back 0 in that state - so this is
    // the one place that actually remembers the real total to restore.
    int knownMaximum_ = 0;
    // Set once, in on_finished() - every other slot checks this first
    // and no-ops if true. See on_finished()'s own comment for the real
    // crash this guards against.
    bool finished_ = false;
    // Stored so on_finished() can sever worker_'s connections into this
    // dialog explicitly (worker_->disconnect(this)) instead of relying
    // only on finished_. A PREVIOUS fix attempt here called a bare
    // disconnect() (this->disconnect(), no args) intending the same
    // thing, but that disconnects signals *this* object emits, not the
    // worker->this connections actually in play - wrong direction,
    // fixed nothing, and produced a spurious "wildcard call disconnects
    // from destroyed signal" warning. worker_->disconnect(this)
    // disconnects every connection where worker_ is the sender AND this
    // is the receiver - the actual ones set up in bindWorker_() - which
    // should stop any FUTURE emit from worker_ from even posting a new
    // event to this object at all, regardless of GUI-thread deletion
    // timing.
    //
    // QPointer, NOT a raw ThreadedIO* - real crash this fixes: rebind()
    // reused this dialog for a SECOND import while worker_ still pointed
    // at the FIRST import's worker, which by then had already been
    // deleteLater()'d and destroyed by CanvasView (on_insert_images_
    // finished()) - a plain ThreadedIO* has no idea its pointee died,
    // so QObject::disconnect(worker_, ...) in rebind() dereferenced
    // freed memory (confirmed via a real backtrace crashing inside
    // QObject::disconnect() itself, called from here). QPointer clears
    // itself back to nullptr automatically the moment its pointee is
    // destroyed, so every `if (worker_)` check stays honest even across
    // a worker that died since the last time this was touched.
    QPointer<ThreadedIO> worker_;
    // false by default - see setReusable()'s own doc comment above.
    bool reusable_ = false;
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

        // ─── Level filter checkboxes ────────────────────────────────────
        // FlatCheckBox (this app's own hand-painted checkbox - a plain
        // QCheckBox + QSS silently loses its native indicator entirely,
        // see widgets/flat_checkbox.h) instead of a bare QCheckBox, same
        // as every other checkbox in this app. Colors come from this
        // dialog's own live QPalette (this dialog still follows the OS
        // theme, unlike e.g. the Settings window's fixed palette) - the
        // accent per level reuses familiar::dialog_style::severityColor()'s
        // established amber/red/blue rather than inventing new ones, so a
        // checked box's own fill color already reads as that level's
        // severity, no separate icon needed. All checked by default -
        // unfiltered, same as before this was added. Unchecking one hides
        // it from BOTH the already-buffered text and any new line that
        // comes in at that level, without discarding it - allEntries_
        // still keeps every line so toggling a box back on brings it
        // right back.
        auto* filterRow = new QHBoxLayout();
        filterRow->addWidget(new QLabel(tr("Show:")));
        const QColor text = palette().color(QPalette::WindowText);
        const QColor border = palette().color(QPalette::Mid);
        const QColor warnAccent
            = familiar::dialog_style::severityColor(QMessageBox::Warning, border);
        const QColor errorAccent
            = familiar::dialog_style::severityColor(QMessageBox::Critical, border);
        struct LevelEntry
        {
            familiar::log::Level level;
            QString label;
            QColor accent;
        };
        // Trace/Debug: no real "severity" of their own, so no color from
        // severityColor() either (unlike Info/Warning/Error/Critical,
        // which map onto its existing QMessageBox::Icon cases) - `text`
        // (WindowText), not `border` (Mid): checked fills with the accent
        // SOLID (see FlatCheckBox::paintEvent() - both pen and brush),
        // and Mid has no contrast guarantee against Window on every
        // style/theme (confirmed - it visually vanished into the
        // dialog's own background on a real run). WindowText/Window are
        // the one pair QPalette actually guarantees will contrast.
        // Critical reuses Error's red, darkened - severityColor() itself
        // has no separate "more severe than Critical" case to draw on.
        const QList<LevelEntry> kLevels = {
            {familiar::log::Level::Trace, tr("Trace"), text},
            {familiar::log::Level::Debug, tr("Debug"), text},
            {familiar::log::Level::Info,
             tr("Info"),
             familiar::dialog_style::severityColor(QMessageBox::Information,
                                                   border)},
            {familiar::log::Level::Warning, tr("Warning"), warnAccent},
            {familiar::log::Level::Error, tr("Error"), errorAccent},
            {familiar::log::Level::Critical,
             tr("Critical"),
             errorAccent.darker(130)},
        };
        for (const LevelEntry& entry : kLevels) {
            const familiar::log::Level level = entry.level;
            auto* box = new FlatCheckBox(entry.label,
                                         text,
                                         border,
                                         entry.accent,
                                         this);
            box->setChecked(true);
            visibleLevels_[level] = true;
            connect(box, &QCheckBox::toggled, this, [this, level](bool on) {
                visibleLevels_[level] = on;
                refreshDisplay();
            });
            filterRow->addWidget(box);
        }
        filterRow->addStretch(1);

        if (familiar::log::RingSink* ring = familiar::log::ringSink()) {
            allEntries_ = ring->entries();
            refreshDisplay();
            connect(ring,
                    &familiar::log::RingSink::entryAdded,
                    this,
                    &DebugLogDialog::appendEntry);
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
        layout->addLayout(filterRow);
        layout->addWidget(log);
        layout->addWidget(buttons);
        show();
    }

private:
    QPlainTextEdit* log;
    QPushButton* copyButton;
    bool followTail_ = true;
    // Every line seen so far, unfiltered - refreshDisplay() re-derives
    // what's actually shown from this plus visibleLevels_ below, so
    // toggling a checkbox never loses anything already buffered.
    QList<familiar::log::RingSink::Entry> allEntries_;
    QMap<familiar::log::Level, bool> visibleLevels_;

private slots:
    // A single new line: cheap append instead of a full rebuild (the
    // common case - most log lines arrive with every filter already
    // settled, not mid-toggle).
    void appendEntry(familiar::log::Level level, const QString& line)
    {
        allEntries_.append({level, line});
        if (visibleLevels_.value(level, true)) {
            log->appendPlainText(line);
        }
    }

    // Full rebuild from allEntries_ - only needed right after a checkbox
    // toggle (or the initial seed from the ring), not per-line.
    void refreshDisplay()
    {
        QStringList visible;
        visible.reserve(allEntries_.size());
        for (const familiar::log::RingSink::Entry& entry : allEntries_) {
            if (visibleLevels_.value(entry.level, true)) {
                visible.append(entry.line);
            }
        }
        log->setPlainText(visible.join('\n'));
    }

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
