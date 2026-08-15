#include "flat_combobox.h"

#include <QAbstractItemView>
#include <QEvent>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionComboBox>

#include <algorithm>

// ─── FlatComboItemDelegate ──────────────────────────────────────────────────

FlatComboItemDelegate::FlatComboItemDelegate(const QColor& text,
                                             const QColor& highlightBackground,
                                             QObject* parent)
    : QStyledItemDelegate(parent)
    , text_(text)
    , highlightBackground_(highlightBackground)
{}

void FlatComboItemDelegate::paint(QPainter* painter,
                                  const QStyleOptionViewItem& option,
                                  const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // State_Selected covers both an actual mouse hover over a row and
    // the popup's initial "this is the current value" highlight - one
    // solid pill for both, matching PureRef's own popup (Max's
    // reference screenshot), just this window's gray instead of its
    // blue.
    const bool highlighted = option.state & (QStyle::State_Selected
                                             | QStyle::State_MouseOver);
    QColor fg = text_;
    if (highlighted) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(highlightBackground_);
        painter->drawRoundedRect(
            QRectF(option.rect).adjusted(2, 1, -2, -1), 5, 5);
        fg = Qt::white;
    }

    painter->setPen(fg);
    const QRect textRect = option.rect.adjusted(10, 0, -10, 0);
    painter->drawText(textRect,
                      Qt::AlignVCenter | Qt::AlignLeft,
                      index.data(Qt::DisplayRole).toString());

    painter->restore();
}

QSize FlatComboItemDelegate::sizeHint(const QStyleOptionViewItem& option,
                                      const QModelIndex& index) const
{
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    s.setHeight(std::max(s.height(), 32));
    return s;
}

// ─── FlatComboBox ───────────────────────────────────────────────────────────

FlatComboBox::FlatComboBox(const QColor& background,
                           const QColor& text,
                           const QColor& hoverBackground,
                           const QColor& itemHighlight,
                           QWidget* parent)
    : QComboBox(parent)
    , background_(background)
    , text_(text)
    , hoverBackground_(hoverBackground)
{
    view()->setItemDelegate(new FlatComboItemDelegate(text, itemHighlight, this));
    // QAbstractItemView is itself a QFrame - it draws its own native
    // sunken/raised bevel (a highlight line on one edge, a shadow line
    // on the opposite one) independently of the QSS "border:" on
    // "QComboBox QAbstractItemView" (settings_style.cpp), which only
    // styles the box-model border, not the frame's own native drawFrame()
    // pass. That bevel is what showed up as stray horizontal lines top
    // and bottom of the popup (Max, by screenshot).
    view()->setFrameShape(QFrame::NoFrame);
    // view() (QAbstractItemView) paints its own rounded background fine
    // via the QSS on "QComboBox QAbstractItemView" (settings_style.cpp),
    // but its viewport() is a separate CHILD widget that auto-fills ITS
    // OWN background as a plain square, sitting on top - same class of
    // bug as titleBar overpainting SettingsWindow's rounded corners
    // (ui/settings_window.cpp) - a square patch was showing through
    // behind the rounded pill/corners (Max, by screenshot). Letting the
    // view's own rounded background show through instead.
    view()->viewport()->setAutoFillBackground(false);
}

void FlatComboBox::enterEvent(QEnterEvent* event)
{
    QComboBox::enterEvent(event);
    update();
}

void FlatComboBox::leaveEvent(QEvent* event)
{
    QComboBox::leaveEvent(event);
    update();
}

void FlatComboBox::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const bool enabled = isEnabled();
    const QColor bg = (enabled && underMouse()) ? hoverBackground_ : background_;
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(rect(), 6, 6);

    QStyleOptionComboBox opt;
    initStyleOption(&opt);
    const QRect arrowRect = style()->subControlRect(QStyle::CC_ComboBox,
                                                     &opt,
                                                     QStyle::SC_ComboBoxArrow,
                                                     this);
    const QRect editRect = style()->subControlRect(QStyle::CC_ComboBox,
                                                    &opt,
                                                    QStyle::SC_ComboBoxEditField,
                                                    this);

    QColor fg = text_;
    if (!enabled)
        fg.setAlpha(110);

    p.setPen(fg);
    const QRect textRect = editRect.adjusted(4, 0, -4, 0);
    p.drawText(textRect,
              Qt::AlignVCenter | Qt::AlignLeft,
              fontMetrics().elidedText(currentText(),
                                       Qt::ElideRight,
                                       textRect.width()));

    const qreal cx = arrowRect.center().x();
    const qreal cy = arrowRect.center().y();
    constexpr qreal halfW = 4.0;
    constexpr qreal halfH = 2.5;
    QPainterPath arrow;
    arrow.moveTo(cx - halfW, cy - halfH);
    arrow.lineTo(cx + halfW, cy - halfH);
    arrow.lineTo(cx, cy + halfH);
    arrow.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(fg);
    p.drawPath(arrow);
}
