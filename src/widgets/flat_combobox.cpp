#include "flat_combobox.h"

#include "log/log.h"
#include <algorithm>
#include <widgets/dialog_style.h>
#include <widgets/settings_style.h>
#include <QAbstractItemView>
#include <QEvent>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionComboBox>
#include <qtimer.h>
using namespace familiar::log;
// ─── FlatComboItemDelegate ──────────────────────────────────────────────────

FlatComboItemDelegate::FlatComboItemDelegate(const QColor& text,
                                             const QColor& highlightBackground,
                                             QObject* parent)
    : QStyledItemDelegate(parent)
    , text_(text)
    , highlightBackground_(highlightBackground)
{
    FLOG_DEBUG(Ch::UI, "highlightBackground_ {}", highlightBackground_);
}

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
    const bool highlighted = option.state
                             & (QStyle::State_Selected
                                | QStyle::State_MouseOver);
    if (highlighted) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(highlightBackground_);
        painter->drawRoundedRect(QRectF(option.rect).adjusted(2, 1, -2, -1),
                                 5,
                                 5);
    }

    painter->setPen(text_);
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
    view()->setItemDelegate(
        new FlatComboItemDelegate(text, hoverBackground, this));
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
void FlatComboBox::showPopup()
{
    QComboBox::showPopup();

    QWidget* container = view()->window();
    if (!container || container == this)
        return;

    if (auto* frame = qobject_cast<QFrame*>(container))
        frame->setFrameShape(QFrame::NoFrame);

    container->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint
                              | Qt::NoDropShadowWindowHint);

    const auto& sp = familiar::settings_style::palette();

    // Теперь С border-radius — QSS сам нарисует скруглённую рамку
    container->setStyleSheet(
        QStringLiteral("background: %1;"
                       "border: 1px solid %2;"
                       "border-radius: 6px;")
            .arg(sp.popupBackground.name(), sp.border.name()));

    view()->setStyleSheet("background: transparent; border: none;");
    view()->viewport()->setAutoFillBackground(false);
    view()->viewport()->setStyleSheet("background: transparent;");

    // прячем скроллеры
    for (QObject* child : container->children()) {
        auto* w = qobject_cast<QWidget*>(child);
        if (w
            && QString::fromLatin1(w->metaObject()->className())
                   .contains(QStringLiteral("Scroller"), Qt::CaseInsensitive)) {
            w->hide();
        }
    }

    // Маску ставим после геометрии
    QTimer::singleShot(0, container, [container]() {
        familiar::dialog_style::applyRoundedMask(container, 6);
    });
}

void FlatComboBox::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const bool enabled = isEnabled();
    const QColor bg = (enabled && underMouse()) ? hoverBackground_
                                                : background_;
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
