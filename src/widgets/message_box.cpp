#include "message_box.h"
#include "dialog_style.h"

#include <core/settingshandler.h>

#include <QFont>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWindow>

namespace {

constexpr int kIconSize = 40;

QString buttonLabel(QMessageBox::StandardButton id)
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
        return QString();
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
    , icon_(icon)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    // NOT translucent, despite the rounded corners below - see
    // dialog_style::panelStyleSheet()'s own comment for why (confirmed
    // visually: WA_TranslucentBackground on a genuinely
    // top-level, QSS-auto-painted widget left the whole panel see-
    // through instead of just rounding the corners).
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_StyledBackground);
    setWindowModality(Qt::ApplicationModal);
    setWindowTitle(title);
    setMinimumWidth(340);
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

    iconLabel_ = new QLabel(this);
    iconLabel_->setFixedSize(kIconSize, kIconSize);
    iconLabel_->setPixmap(
        familiar::dialog_style::severityIcon(icon_,
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

    auto* closeBtn = new QPushButton(QStringLiteral("×"), this);
    closeBtn->setFixedSize(22, 22);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFocusPolicy(Qt::NoFocus);
    closeBtn->setObjectName(QStringLiteral("cmbCloseBtn"));
    connect(closeBtn, &QPushButton::clicked, this, &CustomMessageBox::reject);
    topRow->addWidget(closeBtn, 0, Qt::AlignTop);

    outer->addLayout(topRow);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(8);
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
    for (QMessageBox::StandardButton b : kPriority) {
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
        auto* btn = new QPushButton(buttonLabel(id), this);
        btn->setFocusPolicy(Qt::StrongFocus);
        btn->setDefault(isPrimary);
        if (isPrimary) {
            familiar::dialog_style::stylePrimaryButton(btn, accent);
        } else {
            familiar::dialog_style::styleSecondaryButton(btn, textColor, border);
        }
        connect(btn, &QPushButton::clicked, this, [this, id] { done(int(id)); });
        buttonRow->addWidget(btn);
        if (isPrimary) {
            btn->setFocus();
        }
    };

    for (QMessageBox::StandardButton id : order) {
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

    setStyleSheet(familiar::dialog_style::panelStyleSheet("CustomMessageBox",
                                                          background,
                                                          border,
                                                          textColor)
                  + familiar::dialog_style::closeButtonStyleSheet("cmbCloseBtn",
                                                                  textColor,
                                                                  accent));
}

void CustomMessageBox::setIconPixmap(const QPixmap& pixmap)
{
    iconLabel_->setPixmap(pixmap.scaled(kIconSize * devicePixelRatioF(),
                                        kIconSize * devicePixelRatioF(),
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

void CustomMessageBox::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    familiar::dialog_style::applyRoundedMask(this, 10);
}

void CustomMessageBox::reject()
{
    done(int(escapeButton_));
}

QMessageBox::StandardButton showMessageBox(
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
