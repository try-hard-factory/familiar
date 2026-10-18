#include "flat_checkbox.h"

#include <QEvent>
#include <QPainter>
#include <QPainterPath>

namespace {
constexpr int kindicatorSize = 15;

// Row height, so the indicator has breathing room above and below.
constexpr int kminRowHeight = 22;
// Box outline and the tick drawn inside it.
constexpr qreal kboxStroke = 1.5;
constexpr qreal kboxRadius = 4;
constexpr qreal ktickStroke = 2.0;
// Hover brightens the accent fill and the unchecked outline.
constexpr int khoverLightenPercent = 115;
constexpr int kborderAlphaHover = 200;
constexpr int kborderAlpha = 130;
// The tick, as fractions of the box so it scales with kindicatorSize.
constexpr qreal ktickStartX = 0.22;
constexpr qreal ktickStartY = 0.52;
constexpr qreal ktickMidX = 0.42;
constexpr qreal ktickMidY = 0.74;
constexpr qreal ktickEndX = 0.80;
constexpr qreal ktickEndY = 0.26;
} // namespace

FlatCheckBox::FlatCheckBox(const QString& text,
                           const QColor& textColor,
                           const QColor& border,
                           const QColor& accent,
                           QWidget* parent)
    : QCheckBox(text, parent)
    , textColor_(textColor)
    , border_(border)
    , accent_(accent)
{
    setMinimumHeight(kminRowHeight);
    setAttribute(Qt::WA_NoSystemBackground, true);
    connect(this, &QCheckBox::toggled, this, qOverload<>(&QWidget::update));
}

void FlatCheckBox::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int size = kindicatorSize;
    const QRectF box(0, (height() - size) / 2.0, size, size);

    // Deliberately NOT a one-line `cond ? accent_ : Qt::NoBrush` ternary
    // here - QColor and Qt::BrushStyle have no common type Qt intends
    // for that, and the compiler silently picked QColor as the common
    // type (Qt::NoBrush's underlying 0 implicitly became QRgb 0 ->
    // QColor(0) -> OPAQUE BLACK, not "no brush"), so the unchecked box
    // painted as a solid black square instead of an outline (confirmed
    // visually - alpha tweaks on the border color did
    // nothing because the BRUSH, not the pen, was the real culprit).
    if (isChecked()) {
        const QColor fill = underMouse()
                                ? accent_.lighter(khoverLightenPercent)
                                : accent_;
        p.setPen(QPen(fill, kboxStroke));
        p.setBrush(fill);
    } else {
        QColor uncheckedBorder = border_;
        uncheckedBorder.setAlpha(underMouse() ? kborderAlphaHover
                                              : kborderAlpha);
        p.setPen(QPen(uncheckedBorder, kboxStroke));
        p.setBrush(Qt::NoBrush);
    }
    p.drawRoundedRect(box, kboxRadius, kboxRadius);

    if (isChecked()) {
        QPen checkPen(Qt::white);
        checkPen.setWidthF(ktickStroke);
        checkPen.setCapStyle(Qt::RoundCap);
        checkPen.setJoinStyle(Qt::RoundJoin);
        p.setPen(checkPen);
        QPainterPath check;
        check.moveTo(box.left() + (size * ktickStartX),
                     box.top() + (size * ktickStartY));
        check.lineTo(box.left() + (size * ktickMidX),
                     box.top() + (size * ktickMidY));
        check.lineTo(box.left() + (size * ktickEndX),
                     box.top() + (size * ktickEndY));
        p.drawPath(check);
    }

    p.setPen(textColor_);
    const QRectF textRect(box.right() + 8,
                          0,
                          width() - box.right() - 8,
                          height());
    p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text());
}

void FlatCheckBox::enterEvent(QEnterEvent* event)
{
    QCheckBox::enterEvent(event);
    update();
}

void FlatCheckBox::leaveEvent(QEvent* event)
{
    QCheckBox::leaveEvent(event);
    update();
}
