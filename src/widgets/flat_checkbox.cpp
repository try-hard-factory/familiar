#include "flat_checkbox.h"

#include <QEvent>
#include <QPainter>
#include <QPainterPath>

namespace {
constexpr int kIndicatorSize = 15;
}

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
    setMinimumHeight(22);
    setAttribute(Qt::WA_NoSystemBackground, true);
    connect(this, &QCheckBox::toggled, this, qOverload<>(&QWidget::update));
}

void FlatCheckBox::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int size = kIndicatorSize;
    const QRectF box(0, (height() - size) / 2.0, size, size);

    // Deliberately NOT a one-line `cond ? accent_ : Qt::NoBrush` ternary
    // here - QColor and Qt::BrushStyle have no common type Qt intends
    // for that, and the compiler silently picked QColor as the common
    // type (Qt::NoBrush's underlying 0 implicitly became QRgb 0 ->
    // QColor(0) -> OPAQUE BLACK, not "no brush"), so the unchecked box
    // painted as a solid black square instead of an outline (Max,
    // confirmed via screenshot - alpha tweaks on the border color did
    // nothing because the BRUSH, not the pen, was the real culprit).
    if (isChecked()) {
        const QColor fill = underMouse() ? accent_.lighter(115) : accent_;
        p.setPen(QPen(fill, 1.5));
        p.setBrush(fill);
    } else {
        QColor uncheckedBorder = border_;
        uncheckedBorder.setAlpha(underMouse() ? 200 : 130);
        p.setPen(QPen(uncheckedBorder, 1.5));
        p.setBrush(Qt::NoBrush);
    }
    p.drawRoundedRect(box, 4, 4);

    if (isChecked()) {
        QPen checkPen(Qt::white);
        checkPen.setWidthF(2.0);
        checkPen.setCapStyle(Qt::RoundCap);
        checkPen.setJoinStyle(Qt::RoundJoin);
        p.setPen(checkPen);
        QPainterPath check;
        check.moveTo(box.left() + size * 0.22, box.top() + size * 0.52);
        check.lineTo(box.left() + size * 0.42, box.top() + size * 0.74);
        check.lineTo(box.left() + size * 0.80, box.top() + size * 0.26);
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
