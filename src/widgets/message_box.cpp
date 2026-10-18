#include "message_box.h"
#include "dialog_style.h"

#include <core/settingshandler.h>

#include <QColor>
#include <QDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QMessageBox>
#include <QMouseEvent>
#include <QObject>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWindow>
#include <Qt>

namespace {

constexpr int kiconSize = 40;

// Width band: wide enough for a file path, capped so a long one wraps
// instead of stretching the dialog across the screen.
constexpr int kminDialogWidth = 340;
constexpr int kmaxDialogWidth = 460;
constexpr int kouterSpacing = 14;
constexpr int ktopRowSpacing = 12;
constexpr int kbuttonRowSpacing = 8;

QString button_label(QMessageBox::StandardButton id)
{
    switch (id) {
    case QMessageBox::Ok:
        return CustomMessageBox::tr("OK");
    case QMessageBox::Yes:
        return CustomMessageBox::tr("Yes");
    case QMessageBox::No:
        return CustomMessageBox::tr("No");
    case QMessageBox::Cancel:
        return CustomMessageBox::tr("Cancel");
    case QMessageBox::Discard:
        return CustomMessageBox::tr("Discard");
    default:
        return {};
    }
}

} // namespace

CustomMessageBox::CustomMessageBox(QMessageBox::Icon icon,
                                   QWidget* parent,
                                   const QString& title,
                                   const QString& text,
                                   QMessageBox::StandardButtons buttons,
                                   QMessageBox::StandardButton defaultButton)
    : QDialog(parent)
    , iconLabel_(new QLabel(this))
    , icon_(icon)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    // NOT translucent (unrelated to the square corners below, still
    // true even for an opaque panel) - see
    // dialog_style::panelStyleSheet()'s own comment for why (confirmed
    // visually: WA_TranslucentBackground on a genuinely
    // top-level, QSS-auto-painted widget left the whole panel see-
    // through instead of just rounding the corners).
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_StyledBackground);
    setWindowModality(Qt::ApplicationModal);
    setWindowTitle(title);
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


    iconLabel_->setFixedSize(kiconSize, kiconSize);
    iconLabel_->setPixmap(
        familiar::dialog_style::severity_icon(icon_,
                                              accent,
                                              devicePixelRatioF()));
    topRow->addWidget(iconLabel_, 0, Qt::AlignTop);

    auto* textCol = new QVBoxLayout();
    textCol->setSpacing(4);
    auto* titleLabel = new QLabel(title, this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    titleLabel->setFont(titleFont);
    textCol->addWidget(titleLabel);

    auto* textLabel = new QLabel(text, this);
    textLabel->setWordWrap(true);
    textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    textCol->addWidget(textLabel);
    topRow->addLayout(textCol, 1);

    auto* closeBtn = familiar::dialog_style::make_close_button(this,
                                                               "cmbCloseBtn");
    connect(closeBtn, &QPushButton::clicked, this, &CustomMessageBox::reject);
    topRow->addWidget(closeBtn, 0, Qt::AlignTop);

    outer->addLayout(topRow);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(kbuttonRowSpacing);
    buttonRow->addStretch();

    // Fixed rendering priority (left to right) - whichever of these are
    // actually requested via `buttons`, in this order - the caller's
    // defaultButton (if any) is then pulled out and re-added LAST, so
    // the "primary" action always ends up rightmost/most prominent,
    // matching the accent-fill-vs-outline styling below. If the caller
    // didn't specify one, the naturally-last button in priority order
    // (typically the affirmative one - Yes/Ok) becomes primary instead.
    static const QMessageBox::StandardButton kPriority[] = {
        QMessageBox::Yes,
        QMessageBox::No,
        QMessageBox::Ok,
        QMessageBox::Discard,
        QMessageBox::Cancel,
    };
    QList<QMessageBox::StandardButton> order;
    for (const QMessageBox::StandardButton b : kPriority) {
        if (buttons & b) {
            order.append(b);
        }
    }
    QMessageBox::StandardButton primary = defaultButton;
    if (primary == QMessageBox::NoButton && !order.isEmpty()) {
        primary = order.last();
    }
    if (primary != QMessageBox::NoButton) {
        order.removeOne(primary);
    }

    auto addButton = [&](QMessageBox::StandardButton id, bool isPrimary) {
        auto* btn = new QPushButton(button_label(id), this);
        btn->setFocusPolicy(Qt::StrongFocus);
        btn->setDefault(isPrimary);
        if (isPrimary) {
            familiar::dialog_style::style_primary_button(btn, accent);
        } else {
            familiar::dialog_style::style_secondary_button(btn,
                                                           textColor,
                                                           border);
        }
        connect(btn, &QPushButton::clicked, this, [this, id] { done(int(id)); });
        buttonRow->addWidget(btn);
        if (isPrimary) {
            btn->setFocus();
        }
    };

    for (const QMessageBox::StandardButton id : order) {
        addButton(id, false);
    }
    if (primary != QMessageBox::NoButton) {
        addButton(primary, true);
    }

    outer->addLayout(buttonRow);

    if (buttons & QMessageBox::Cancel) {
        escapeButton_ = QMessageBox::Cancel;
    } else if (buttons & QMessageBox::No) {
        escapeButton_ = QMessageBox::No;
    } else {
        escapeButton_ = defaultButton;
    }

    setStyleSheet(
        familiar::dialog_style::panel_style_sheet("CustomMessageBox",
                                                  background,
                                                  border,
                                                  textColor,
                                                  /*radiusPx=*/0)
        + familiar::dialog_style::close_button_style_sheet("cmbCloseBtn",
                                                           textColor,
                                                           accent));
}

void CustomMessageBox::set_icon_pixmap(const QPixmap& pixmap)
{
    const int scaledSize = static_cast<int>(kiconSize * devicePixelRatioF());
    iconLabel_->setPixmap(pixmap.scaled(scaledSize,
                                        scaledSize,
                                        Qt::KeepAspectRatio,
                                        Qt::SmoothTransformation));
}

void CustomMessageBox::mousePressEvent(QMouseEvent* event)
{
    // Frameless, so this IS the title bar for drag purposes - same
    // idiom MainWindow::tryStartWindowDrag_() uses for its own
    // frameless window. Anywhere on the panel works (not just an empty
    // "title strip") since there's nothing else here that itself wants
    // a plain left-press (buttons/close glyph already consume their own
    // clicks before this base handler runs).
    if (event->button() == Qt::LeftButton && windowHandle()) {
        windowHandle()->startSystemMove();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void CustomMessageBox::reject()
{
    done(int(escapeButton_));
}

QMessageBox::StandardButton show_message_box(
    QMessageBox::Icon icon,
    QWidget* parent,
    const QString& title,
    const QString& text,
    QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton defaultButton)
{
    CustomMessageBox box(icon, parent, title, text, buttons, defaultButton);
    return static_cast<QMessageBox::StandardButton>(box.exec());
}
