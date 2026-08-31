#include "raw_import_dialog.h"
#include "dialog_style.h"
#include "flat_checkbox.h"

#include <core/settingshandler.h>

#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWindow>

namespace {
constexpr int kIconSize = 40;

// Items/image_storage_format's own values ("best"/"png"/"jpg" - see
// core/settings.cpp) - this setting has no dedicated UI row of its own
// (dropped during an earlier settings-window redesign, see
// ui/settings_window.cpp's own comment on that), so this dialog is the
// only place that currently surfaces it to the user at all.
QString storageFormatLabel(const QString& format)
{
    if (format == QLatin1String("png")) {
        return QStringLiteral("PNG");
    }
    if (format == QLatin1String("jpg")) {
        return QStringLiteral("JPG");
    }
    return QStringLiteral("PNG/JPG"); // "best"
}

} // namespace

RawImportDialog::RawImportDialog(QWidget* parent, const QString& filename)
    : QDialog(parent)
{
    // Same reasoning as RestoreDefaultsDialog's identical comment - NOT
    // WA_DeleteOnClose, this is used modally (exec()) and that
    // combination is a documented use-after-free risk. Callers
    // (on_raw_import_choice_required(), canvasview.cpp) stack-allocate
    // this.
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_StyledBackground);
    setWindowModality(Qt::ApplicationModal);
    setWindowTitle(tr("RAW file needs converting"));
    setMinimumWidth(380);
    setMaximumWidth(460);

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
    outer->setContentsMargins(20, 16, 16, 16);
    outer->setSpacing(14);

    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(12);

    auto* iconLabel = new QLabel(this);
    iconLabel->setFixedSize(kIconSize, kIconSize);
    iconLabel->setPixmap(
        familiar::dialog_style::severityIcon(QMessageBox::Information,
                                             accent,
                                             devicePixelRatioF()));
    topRow->addWidget(iconLabel, 0, Qt::AlignTop);

    auto* textCol = new QVBoxLayout();
    textCol->setSpacing(4);
    auto* titleLabel = new QLabel(tr("RAW file needs converting"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    titleLabel->setFont(titleFont);
    textCol->addWidget(titleLabel);

    const QString baseName = QFileInfo(filename).fileName();
    auto* textLabel = new QLabel(
        tr("%1 is a RAW camera file - familiar needs to convert it "
           "before it can go on the canvas.")
            .arg(QStringLiteral("<span style=\"color:%1;\">%2</span>")
                     .arg(accent.name(), baseName.toHtmlEscaped())),
        this);
    textLabel->setTextFormat(Qt::RichText);
    textLabel->setWordWrap(true);
    textCol->addWidget(textLabel);
    topRow->addLayout(textCol, 1);

    auto* closeBtn = new QPushButton(QStringLiteral("×"), this);
    closeBtn->setFixedSize(22, 22);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFocusPolicy(Qt::NoFocus);
    closeBtn->setObjectName(QStringLiteral("ridCloseBtn"));
    connect(closeBtn, &QPushButton::clicked, this, &RawImportDialog::reject);
    topRow->addWidget(closeBtn, 0, Qt::AlignTop);

    outer->addLayout(topRow);

    // "Format: NEF -> JPG" chip - same rounded-box language the app's
    // other dialogs use for a single highlighted fact (e.g. this
    // window's own filename above, styled inline instead of boxed, since
    // there's already a title+icon here to anchor it).
    auto* formatChip = new QLabel(this);
    const QString sourceExt = QFileInfo(filename).suffix().toUpper();
    const QString targetFormat = storageFormatLabel(
        SettingsHandler::getInstance()->imageStorageFormat());
    formatChip->setText(tr("Format: %1 → %2").arg(sourceExt, targetFormat));
    formatChip->setStyleSheet(
        QStringLiteral("QLabel { background: %1; border-radius: 6px; "
                       "padding: 8px 12px; }")
            .arg(border.name()));
    outer->addWidget(formatChip);

    auto* noteLabel = new QLabel(
        tr("\"Optimize image\" runs a quicker RAW conversion (fast). "
           "\"Keep original\" runs the best-quality conversion instead "
           "(slower) - either way, this converted copy is what actually "
           "gets saved in the project, not the original file itself."),
        this);
    noteLabel->setWordWrap(true);
    outer->addWidget(noteLabel);

    applyToQueueCheckbox_ = new FlatCheckBox(tr("Apply choice to this queue"),
                                             textColor,
                                             border,
                                             accent,
                                             this);
    outer->addWidget(applyToQueueCheckbox_);

    rememberCheckbox_ = new FlatCheckBox(tr("Remember choice for future files"),
                                         textColor,
                                         border,
                                         accent,
                                         this);
    outer->addWidget(rememberCheckbox_);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(8);
    buttonRow->addStretch();

    auto* keepOriginalBtn = new QPushButton(tr("Keep original"), this);
    familiar::dialog_style::styleSecondaryButton(keepOriginalBtn, textColor, border);
    connect(keepOriginalBtn, &QPushButton::clicked, this, [this]() {
        choice_ = RawImportChoice::KeepOriginal;
        accept();
    });
    buttonRow->addWidget(keepOriginalBtn);

    auto* optimizeBtn = new QPushButton(tr("Optimize image"), this);
    familiar::dialog_style::stylePrimaryButton(optimizeBtn, accent);
    connect(optimizeBtn, &QPushButton::clicked, this, [this]() {
        choice_ = RawImportChoice::Optimize;
        accept();
    });
    buttonRow->addWidget(optimizeBtn);

    outer->addLayout(buttonRow);

    setStyleSheet(
        familiar::dialog_style::panelStyleSheet("RawImportDialog",
                                                background,
                                                border,
                                                textColor,
                                                /*radiusPx=*/0)
        + familiar::dialog_style::closeButtonStyleSheet("ridCloseBtn",
                                                        textColor,
                                                        accent));
}

bool RawImportDialog::applyToQueue() const
{
    return applyToQueueCheckbox_->isChecked();
}

bool RawImportDialog::rememberChoice() const
{
    return rememberCheckbox_->isChecked();
}

void RawImportDialog::mousePressEvent(QMouseEvent* event)
{
    // Frameless, so this IS the title bar for drag purposes - same
    // idiom every other custom top-level dialog in this app uses.
    if (event->button() == Qt::LeftButton && windowHandle()) {
        windowHandle()->startSystemMove();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}
