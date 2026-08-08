#include "group_toolbar.h"

#include <core/settingshandler.h>
#include <moveitem.h>

#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kButtonSize = 30;
constexpr int kIconSize = 18;

// Simple drawn padlock glyph - shackle (arc) + body (rounded rect), same
// QPainter-drawn-icon approach as gif_playback_toolbar.cpp's makeStepIcon()
// etc.
QIcon makeLockIcon(const QColor& glyphColor, qreal dpr)
{
    QPixmap pm(QSize(kIconSize, kIconSize) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    QPen shacklePen(glyphColor);
    shacklePen.setWidthF(2.0);
    shacklePen.setCapStyle(Qt::RoundCap);
    p.setPen(shacklePen);
    p.setBrush(Qt::NoBrush);
    QPainterPath shackle;
    const QRectF shackleRect(kIconSize * 0.28,
                             kIconSize * 0.12,
                             kIconSize * 0.44,
                             kIconSize * 0.44);
    shackle.arcMoveTo(shackleRect, 0);
    shackle.arcTo(shackleRect, 0, 180);
    p.drawPath(shackle);

    p.setPen(Qt::NoPen);
    p.setBrush(glyphColor);
    p.drawRoundedRect(QRectF(kIconSize * 0.18,
                             kIconSize * 0.46,
                             kIconSize * 0.64,
                             kIconSize * 0.42),
                      2.0,
                      2.0);

    p.end();
    QIcon icon;
    icon.addPixmap(pm);
    return icon;
}

} // namespace

GroupToolbar::GroupToolbar(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(18);
    shadow->setOffset(0, 3);
    shadow->setColor(QColor(0, 0, 0, 140));
    setGraphicsEffect(shadow);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* row = new QWidget(this);
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(8, 5, 8, 5);
    lay->setSpacing(2);

    lockBtn_ = new QToolButton(row);
    lockBtn_->setToolTip(tr("Lock group"));
    lockBtn_->setCheckable(true);
    lockBtn_->setAutoRaise(true);
    lockBtn_->setFixedSize(kButtonSize, kButtonSize);
    lockBtn_->setFocusPolicy(Qt::NoFocus);
    lay->addWidget(lockBtn_);

    outer->addWidget(row);

    connect(lockBtn_,
            &QToolButton::toggled,
            this,
            &GroupToolbar::onLockToggled_);

    restyleFromPreset();
}

void GroupToolbar::attach(GroupItem* item)
{
    item_ = item;
    if (item_) {
        const QSignalBlocker blocker(lockBtn_);
        lockBtn_->setChecked(item_->locked());
    }
}

void GroupToolbar::onLockToggled_(bool checked)
{
    if (item_)
        item_->set_locked(checked);
}

void GroupToolbar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    emit geometryChanged();
}

void GroupToolbar::restyleFromPreset()
{
    auto colorPreset = SettingsHandler::getInstance()->getCurrentColorPreset();
    const QColor& text = colorPreset[EPresetsColorIdx::kTextColor];
    const QColor& background = colorPreset[EPresetsColorIdx::kBackgroundColor];
    const QColor& border = colorPreset[EPresetsColorIdx::kBorderColor];
    const QColor& selection = colorPreset[EPresetsColorIdx::kSelectionColor];
    iconGlyphColor_ = text;
    auto rgba = [](const QColor& c, int alpha) {
        return QStringLiteral("rgba(%1, %2, %3, %4)")
            .arg(c.red())
            .arg(c.green())
            .arg(c.blue())
            .arg(alpha);
    };

    setStyleSheet(
        QStringLiteral("GroupToolbar > QWidget {"
                       "  background-color: %1;"
                       "  border: 1px solid %2;"
                       "  border-radius: 8px;"
                       "}"
                       "QToolButton {"
                       "  background: transparent;"
                       "  color: %3;"
                       "  border: none;"
                       "  border-radius: 5px;"
                       "}"
                       "QToolButton:hover { background-color: %4; }"
                       "QToolButton:pressed { background-color: %5; }"
                       "QToolButton:checked { background-color: %5; }"
                       "QToolTip {"
                       "  background-color: %6;"
                       "  color: %3;"
                       "  border: 1px solid %2;"
                       "}")
            .arg(rgba(background, 245),
                 border.name(),
                 text.name(),
                 rgba(selection, 90),
                 rgba(selection, 170),
                 background.name()));

    lockBtn_->setIcon(makeLockIcon(iconGlyphColor_, devicePixelRatioF()));
}
