#include "telemetry_consent_dialog.h"
#include "dialog_style.h"
#include "flat_checkbox.h"

#include <core/settingshandler.h>
#include <telemetry/telemetry.h>

#include <QCheckBox>
#include <QColor>
#include <QDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMargins>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QWindow>
#include <Qt>

namespace {

constexpr int kiconSize = 40;
// Wider than the confirmation dialogs: this one is a paragraph of prose
// plus a bullet list, and a 340px column turns that into a thin ribbon.
constexpr int kminDialogWidth = 420;
constexpr int kmaxDialogWidth = 520;
constexpr int kouterSpacing = 14;
constexpr int ktopRowSpacing = 12;
constexpr int klistSpacing = 4;
constexpr int kbuttonRowSpacing = 8;
// Between the title, the body paragraph and the bullet list - tighter
// than kouterSpacing so the whole explanation reads as one block.
constexpr int ktextColumnSpacing = 6;

} // namespace

TelemetryConsentDialog::TelemetryConsentDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_StyledBackground);
    setWindowModality(Qt::ApplicationModal);
    setWindowTitle(tr("Anonymous Usage Count"));
    setMinimumWidth(kminDialogWidth);
    setMaximumWidth(kmaxDialogWidth);

    familiar::dialog_style::apply_dialog_shadow(this);

    auto colorPreset
        = SettingsHandler::get_instance()->get_current_color_preset();
    const QColor& textColor = colorPreset[EPresetsColorIdx::TextColor];
    const QColor& background = colorPreset[EPresetsColorIdx::BackgroundColor];
    const QColor& border = colorPreset[EPresetsColorIdx::BorderColor];
    const QColor& accent = colorPreset[EPresetsColorIdx::SelectionColor];

    auto* outer = new QVBoxLayout(this);
    familiar::dialog_style::apply_dialog_margins(outer);
    outer->setSpacing(kouterSpacing);

    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(ktopRowSpacing);

    auto* iconLabel = new QLabel(this);
    iconLabel->setFixedSize(kiconSize, kiconSize);
    iconLabel->setPixmap(
        familiar::dialog_style::severity_icon(QMessageBox::Information,
                                              accent,
                                              devicePixelRatioF()));
    topRow->addWidget(iconLabel, 0, Qt::AlignTop);

    auto* textCol = new QVBoxLayout();
    textCol->setSpacing(ktextColumnSpacing);

    auto* titleLabel = new QLabel(tr("Help count how many people use "
                                     "familiar"),
                                  this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    titleLabel->setFont(titleFont);
    titleLabel->setWordWrap(true);
    textCol->addWidget(titleLabel);

    // Plain wording on purpose: it has to say exactly what leaves the
    // machine, because the checkbox below starts ticked. Someone who
    // reads only this much should already know what they are agreeing
    // to.
    //
    // NOTE FOR REVIEW: this is a plain-language disclosure, not a legal
    // privacy policy. If a formal policy page exists, link it here.
    auto* bodyLabel = new QLabel(
        tr("Once a day, familiar can send two things to its own server:"), this);
    bodyLabel->setWordWrap(true);
    textCol->addWidget(bodyLabel);

    auto* list = new QVBoxLayout();
    list->setSpacing(klistSpacing);
    auto* idLabel = new QLabel(tr("• a random number generated on this "
                                  "computer, which identifies the "
                                  "installation but not you"),
                               this);
    idLabel->setWordWrap(true);
    list->addWidget(idLabel);
    auto* versionLabel = new QLabel(tr("• the version of familiar you are "
                                       "running"),
                                    this);
    versionLabel->setWordWrap(true);
    list->addWidget(versionLabel);
    textCol->addLayout(list);

    auto* notLabel = new QLabel(
        tr("That is all. No file names, no images, no canvas contents, no "
           "name or email. The server keeps no IP addresses and no time of "
           "day — only the date. It is used to count installations and "
           "launches, and to see which versions are still in use."),
        this);
    notLabel->setWordWrap(true);
    textCol->addWidget(notLabel);

    auto* laterLabel = new QLabel(tr("You can change this at any time in "
                                     "Settings."),
                                  this);
    laterLabel->setWordWrap(true);
    textCol->addWidget(laterLabel);

    topRow->addLayout(textCol, 1);

    auto* closeBtn = familiar::dialog_style::make_close_button(this,
                                                               "tcdCloseBtn");
    // accept(), not reject(): the × means "go with what the checkbox
    // says" rather than a separate refusal - see the header's comment.
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    topRow->addWidget(closeBtn, 0, Qt::AlignTop);

    outer->addLayout(topRow);

    enabledCheckbox_ = new FlatCheckBox(tr("Send the anonymous count"),
                                        textColor,
                                        border,
                                        accent,
                                        this);
    enabledCheckbox_->setChecked(familiar::telemetry::default_enabled());
    outer->addWidget(enabledCheckbox_);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(kbuttonRowSpacing);
    buttonRow->addStretch();

    auto* continueBtn = new QPushButton(tr("Continue"), this);
    familiar::dialog_style::style_primary_button(continueBtn, accent);
    continueBtn->setDefault(true);
    connect(continueBtn, &QPushButton::clicked, this, &QDialog::accept);
    buttonRow->addWidget(continueBtn);

    outer->addLayout(buttonRow);

    setStyleSheet(
        familiar::dialog_style::panel_style_sheet("TelemetryConsentDialog",
                                                  background,
                                                  border,
                                                  textColor)
        + familiar::dialog_style::close_button_style_sheet("tcdCloseBtn",
                                                           textColor,
                                                           accent));
}

bool TelemetryConsentDialog::accepted_telemetry() const
{
    return enabledCheckbox_ != nullptr && enabledCheckbox_->isChecked();
}

void TelemetryConsentDialog::mousePressEvent(QMouseEvent* event)
{
    // Frameless, so the panel itself is the title bar for dragging -
    // same idiom as every other custom top-level dialog here.
    if (event->button() == Qt::LeftButton && windowHandle() != nullptr) {
        windowHandle()->startSystemMove();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void TelemetryConsentDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    // QSS border-radius only paints inside the rounded outline; without
    // this the real window corners stay square and show through.
    familiar::dialog_style::apply_rounded_mask(
        this, familiar::dialog_style::kdialogCornerRadius);
}
