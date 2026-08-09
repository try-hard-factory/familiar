#include "message_box.h"

#include <core/settingshandler.h>

#include <QFont>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWindow>

namespace {

constexpr int kIconSize = 40;

// Conventional severity colors (amber/red/blue), fixed regardless of
// the active color preset - these read as universally recognizable
// regardless of theme, the same reasoning real OS/toolkit icon sets
// use. Question has no real "severity", so it just uses the app's own
// accent color instead of inventing a fourth fixed hue.
QColor severityColor(QMessageBox::Icon icon, const QColor& accent)
{
    switch (icon) {
    case QMessageBox::Warning:
        return QColor(0xf5, 0xa6, 0x23);
    case QMessageBox::Critical:
        return QColor(0xe5, 0x48, 0x4d);
    case QMessageBox::Information:
        return QColor(0x4a, 0x90, 0xd9);
    case QMessageBox::Question:
    default:
        return accent;
    }
}

// Warning gets a triangle (matches common OS iconography - "caution"
// reads as a triangle, everything else as a circle), rest a plain
// filled circle with a bold glyph on top. Drawn, not loaded - same
// approach as every other icon in this app (group_toolbar.cpp,
// gif_playback_toolbar.cpp - no external asset, always matches the
// current DPI exactly).
QPixmap makeSeverityIcon(QMessageBox::Icon icon, const QColor& accent, qreal dpr)
{
    QPixmap pm(QSize(kIconSize, kIconSize) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const QColor bg = severityColor(icon, accent);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    if (icon == QMessageBox::Warning) {
        QPainterPath tri;
        tri.moveTo(kIconSize * 0.5, kIconSize * 0.05);
        tri.lineTo(kIconSize * 0.96, kIconSize * 0.92);
        tri.lineTo(kIconSize * 0.04, kIconSize * 0.92);
        tri.closeSubpath();
        p.drawPath(tri);
    } else {
        p.drawEllipse(QRectF(
            kIconSize * 0.05, kIconSize * 0.05, kIconSize * 0.9, kIconSize * 0.9));
    }

    QString glyph;
    switch (icon) {
    case QMessageBox::Warning:
    case QMessageBox::Critical:
        glyph = QStringLiteral("!");
        break;
    case QMessageBox::Information:
        glyph = QStringLiteral("i");
        break;
    case QMessageBox::Question:
        glyph = QStringLiteral("?");
        break;
    default:
        break;
    }
    if (!glyph.isEmpty()) {
        QFont font = p.font();
        font.setBold(true);
        font.setPixelSize(
            int(kIconSize * (icon == QMessageBox::Warning ? 0.38 : 0.48)));
        p.setFont(font);
        p.setPen(Qt::white);
        const QRectF textRect(
            0, icon == QMessageBox::Warning ? kIconSize * 0.12 : 0, kIconSize, kIconSize);
        p.drawText(textRect, Qt::AlignHCenter | Qt::AlignVCenter, glyph);
    }

    p.end();
    return pm;
}

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
    // NOT translucent, despite the rounded corners below - same choice
    // GroupToolbar's own settings popup makes (ui/group_toolbar.cpp) for
    // the same reason: WA_TranslucentBackground on a genuinely top-level
    // widget whose background is auto-painted from QSS (WA_StyledBackground)
    // left the whole panel fully see-through instead of just rounding the
    // corners - confirmed by Max via screenshot. Opaque + border-radius
    // alone reads as very slightly squared corners on some window
    // managers, but actually paints, which is what matters here.
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
    iconLabel_->setPixmap(makeSeverityIcon(icon_, accent, devicePixelRatioF()));
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
        if (buttons & b)
            order.append(b);
    }
    QMessageBox::StandardButton primary = defaultButton;
    if (primary == QMessageBox::NoButton && !order.isEmpty())
        primary = order.last();
    if (primary != QMessageBox::NoButton)
        order.removeOne(primary);

    auto addButton = [&](QMessageBox::StandardButton id, bool isPrimary) {
        auto* btn = new QPushButton(buttonLabel(id), this);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumWidth(76);
        btn->setMinimumHeight(30);
        btn->setFocusPolicy(Qt::StrongFocus);
        btn->setDefault(isPrimary);
        if (isPrimary) {
            btn->setStyleSheet(
                QStringLiteral("QPushButton {"
                               "  background-color: %1;"
                               "  color: white;"
                               "  border: none;"
                               "  border-radius: 6px;"
                               "  padding: 4px 14px;"
                               "  font-weight: 600;"
                               "}"
                               "QPushButton:hover { background-color: %2; }"
                               "QPushButton:pressed { background-color: %3; }")
                    .arg(accent.name(),
                         accent.lighter(115).name(),
                         accent.darker(115).name()));
        } else {
            btn->setStyleSheet(
                QStringLiteral("QPushButton {"
                               "  background-color: transparent;"
                               "  color: %1;"
                               "  border: 1px solid %2;"
                               "  border-radius: 6px;"
                               "  padding: 4px 14px;"
                               "}"
                               "QPushButton:hover { background-color: rgba(255,255,255,18); }"
                               "QPushButton:pressed { background-color: rgba(255,255,255,32); }")
                    .arg(textColor.name(), border.name()));
        }
        connect(btn, &QPushButton::clicked, this, [this, id] { done(int(id)); });
        buttonRow->addWidget(btn);
        if (isPrimary)
            btn->setFocus();
    };

    for (QMessageBox::StandardButton id : order)
        addButton(id, false);
    if (primary != QMessageBox::NoButton)
        addButton(primary, true);

    outer->addLayout(buttonRow);

    if (buttons & QMessageBox::Cancel)
        escapeButton_ = QMessageBox::Cancel;
    else if (buttons & QMessageBox::No)
        escapeButton_ = QMessageBox::No;
    else
        escapeButton_ = defaultButton;

    // The panel itself - rounded rect, opaque (not translucent-tinted:
    // same "dialogs/tooltips have no alpha channel to render against"
    // reasoning as the QToolTip QSS elsewhere in this app), preset-
    // sourced. WA_TranslucentBackground above only exists so the OS-
    // level window has an alpha channel for the ROUNDED CORNERS outside
    // this rect to actually be transparent, not so the panel itself is
    // see-through.
    setStyleSheet(
        QStringLiteral("CustomMessageBox {"
                       "  background-color: %1;"
                       "  border: 1px solid %2;"
                       "  border-radius: 10px;"
                       "}"
                       "QLabel { background: transparent; color: %3; }"
                       "#cmbCloseBtn {"
                       "  background: transparent;"
                       "  color: %3;"
                       "  border: none;"
                       "  border-radius: 11px;"
                       "  font-size: 14px;"
                       "}"
                       "#cmbCloseBtn:hover { background-color: rgba(255,255,255,24); }")
            .arg(background.name(), border.name(), textColor.name()));
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

void CustomMessageBox::reject()
{
    done(int(escapeButton_));
}

QMessageBox::StandardButton showMessageBox(QMessageBox::Icon icon,
                                           QWidget* parent,
                                           const QString& title,
                                           const QString& text,
                                           QMessageBox::StandardButtons buttons,
                                           QMessageBox::StandardButton defaultButton)
{
    CustomMessageBox box(icon, parent, title, text, buttons, defaultButton);
    return static_cast<QMessageBox::StandardButton>(box.exec());
}
