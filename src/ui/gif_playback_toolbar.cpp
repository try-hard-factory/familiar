#include "gif_playback_toolbar.h"

#include <core/settingshandler.h>
#include <moveitem.h>

#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QMovie>
#include <QPainter>
#include <QPixmap>
#include <QResizeEvent>
#include <QShortcut>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kbuttonSize = 30;
constexpr int kiconSize = 18;
constexpr int kthumbSize = 48;

// Simple filled triangle - reused (mirrored/rotated) for prev/play/next,
// same drawn-icon approach as ui/text_edit_toolbar.cpp's makeListIcon()
// etc: a small, theme-colored glyph that doesn't need an external asset.
QIcon make_triangle_icon(const QColor& glyphColor, qreal dpr, bool pointLeft)
{
    QPixmap pm(QSize(kiconSize, kiconSize) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(glyphColor);

    QPolygonF tri;
    if (pointLeft) {
        tri << QPointF(kiconSize - 4, 3)
            << QPointF(kiconSize - 4, kiconSize - 3)
            << QPointF(4, kiconSize / 2.0);
    } else {
        tri << QPointF(4, 3) << QPointF(4, kiconSize - 3)
            << QPointF(kiconSize - 4, kiconSize / 2.0);
    }
    p.drawPolygon(tri);

    p.end();
    QIcon icon;
    icon.addPixmap(pm);
    return icon;
}

// "Skip to previous/next frame": a triangle plus a thin bar against the
// edge it points towards.
QIcon make_step_icon(const QColor& glyphColor, qreal dpr, bool prev)
{
    QPixmap pm(QSize(kiconSize, kiconSize) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(glyphColor);

    QPolygonF tri;
    QRectF bar;
    if (prev) {
        tri << QPointF(kiconSize - 5, 3)
            << QPointF(kiconSize - 5, kiconSize - 3)
            << QPointF(7, kiconSize / 2.0);
        bar = QRectF(3, 3, 2, kiconSize - 6);
    } else {
        tri << QPointF(5, 3) << QPointF(5, kiconSize - 3)
            << QPointF(kiconSize - 7, kiconSize / 2.0);
        bar = QRectF(kiconSize - 5, 3, 2, kiconSize - 6);
    }
    p.drawPolygon(tri);
    p.drawRect(bar);

    p.end();
    QIcon icon;
    icon.addPixmap(pm);
    return icon;
}

QIcon make_pause_icon(const QColor& glyphColor, qreal dpr)
{
    QPixmap pm(QSize(kiconSize, kiconSize) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(glyphColor);
    p.drawRect(QRectF(4, 3, 4, kiconSize - 6));
    p.drawRect(QRectF(kiconSize - 8, 3, 4, kiconSize - 6));

    p.end();
    QIcon icon;
    icon.addPixmap(pm);
    return icon;
}

// "Show all frames": three small stacked rectangles, filmstrip-style.
QIcon make_filmstrip_icon(const QColor& glyphColor, qreal dpr)
{
    QPixmap pm(QSize(kiconSize, kiconSize) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(glyphColor);
    pen.setWidthF(1.6);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    const qreal w = (kiconSize - 6) / 3.0;
    for (int i = 0; i < 3; ++i) {
        p.drawRect(QRectF(3 + (i * w), 4, w - 2, kiconSize - 8));
    }

    p.end();
    QIcon icon;
    icon.addPixmap(pm);
    return icon;
}

} // namespace

GifPlaybackToolbar::GifPlaybackToolbar(QWidget* parent)
    : QWidget(parent)
    , filmstrip_(new QWidget(this))
    , filmstripLay_(new QHBoxLayout(filmstrip_))
    , outerLay_(new QVBoxLayout(this))
    , controlsRow_(new QWidget(this))
{
    setAttribute(Qt::WA_StyledBackground);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(18);
    shadow->setOffset(0, 3);
    shadow->setColor(QColor(0, 0, 0, 140));
    setGraphicsEffect(shadow);


    outerLay_->setContentsMargins(0, 0, 0, 0);
    outerLay_->setSpacing(6);


    auto* row = controlsRow_;
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(8, 5, 8, 5);
    lay->setSpacing(2);

    auto makeButton = [row, lay](const QString& tooltip, bool checkable) {
        auto* b = new QToolButton(row);
        b->setToolTip(tooltip);
        b->setCheckable(checkable);
        b->setAutoRaise(true);
        b->setFixedSize(kbuttonSize, kbuttonSize);
        b->setFocusPolicy(Qt::NoFocus);
        lay->addWidget(b);
        return b;
    };

    speedBtn_ = makeButton(tr("Playback speed"), false);
    // Not the same fixed 30x30 square as the icon-only buttons below -
    // "x1.75"/"x0.25" don't fit in that width and Qt elides the label to
    // "...". Size for the widest speed label instead, height still
    // matching the row.
    speedBtn_->setFixedSize(speedBtn_->fontMetrics().horizontalAdvance(
                                QStringLiteral("x0.25"))
                                + 16,
                            kbuttonSize);
    speedBtn_->setText(QStringLiteral("x1"));

    prevBtn_ = makeButton(tr("Previous frame"), false);
    playPauseBtn_ = makeButton(tr("Play / Pause"), false);
    nextBtn_ = makeButton(tr("Next frame"), false);
    framesBtn_ = makeButton(tr("Show all frames"), true);

    // Fixed, not stretched - every button in it already has setFixedSize,
    // so this is its one true width. Without this, the outer QVBoxLayout
    // stretches "row" to match the filmstrip's width once it's shown (the
    // layout sizes every child to the widest one), which is the opposite
    // of what we want: the filmstrip should be as wide as it needs to be,
    // but the control row above it should stay a compact pill, not
    // balloon out to match it.
    //
    // Left-aligned here rather than centered - the real horizontal
    // position is set by positionControlsRow(), called by the owner
    // (CanvasView) every time it repositions the toolbar. Left is just
    // this layout's resting state in between those calls.
    row->setFixedWidth(row->sizeHint().width());
    outerLay_->addWidget(row, 0, Qt::AlignLeft);


    filmstripLay_->setContentsMargins(6, 4, 6, 6);
    filmstripLay_->setSpacing(4);
    filmstrip_->hide();
    outerLay_->addWidget(filmstrip_);

    connect(prevBtn_, &QToolButton::clicked, this, [this] {
        if (item_) {
            item_->step_frame(-1);
        }
    });
    connect(nextBtn_, &QToolButton::clicked, this, [this] {
        if (item_) {
            item_->step_frame(1);
        }
    });
    connect(playPauseBtn_, &QToolButton::clicked, this, [this] {
        if (item_) {
            item_->toggle_play_pause();
        }
        update_play_pause_icon();
    });
    connect(speedBtn_,
            &QToolButton::clicked,
            this,
            &GifPlaybackToolbar::show_speed_popup);
    connect(framesBtn_,
            &QToolButton::clicked,
            this,
            &GifPlaybackToolbar::toggle_filmstrip);

    restyle_from_preset();
}

void GifPlaybackToolbar::attach(GifItem* item)
{
    if (item_ && item_->movie()) {
        disconnect(item_->movie(), nullptr, this, nullptr);
    }
    item_ = item;
    filmstrip_->hide();
    framesBtn_->setChecked(false);
    if (item_) {
        if (item_->movie()) {
            connect(item_->movie(),
                    &QMovie::frameChanged,
                    this,
                    &GifPlaybackToolbar::on_frame_changed);
        }
        update_play_pause_icon();
        update_speed_label();
    }
}

void GifPlaybackToolbar::on_frame_changed(int frame)
{
    Q_UNUSED(frame);
    update_play_pause_icon();
    if (filmstrip_->isVisible()) {
        rebuild_filmstrip(); // cheap enough - just re-highlights the current thumbnail
    }
}

void GifPlaybackToolbar::update_play_pause_icon()
{
    if (!item_) {
        return;
    }
    const qreal dpr = devicePixelRatioF();
    playPauseBtn_->setIcon(
        item_->is_playing() ? make_pause_icon(iconGlyphColor_, dpr)
                            : make_triangle_icon(iconGlyphColor_, dpr, false));
}

void GifPlaybackToolbar::update_speed_label()
{
    if (!item_) {
        return;
    }
    const qreal speed = item_->speed_percent() / 100.0;
    const QString text = QString::number(speed, 'g', 3);
    speedBtn_->setText(QStringLiteral("x%1").arg(text));
}

void GifPlaybackToolbar::show_speed_popup()
{
    if (!item_) {
        return;
    }

    // Reuse an already-open popup instead of stacking a new one on top of
    // it - clicking the speed button repeatedly without picking a speed
    // or hitting Escape used to spawn a fresh QWidget every time.
    if (speedPopup_) {
        speedPopup_->raise();
        speedPopup_->activateWindow();
        return;
    }

    // Qt::Tool, not Qt::Popup - see ui/text_edit_toolbar.cpp's
    // showLinkPopup() for why (a Popup auto-closes, and with
    // WA_DeleteOnClose gets destroyed, the instant it loses activation -
    // not a risk here since this popup never opens a child dialog of its
    // own, but keeping the same widget flags/dismissal convention across
    // both floating toolbars in this app is one less thing to remember).
    auto* popup = new QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setAttribute(Qt::WA_TranslucentBackground, false);
    speedPopup_ = popup;
    connect(popup, &QObject::destroyed, this, [this] {
        this->speedPopup_ = nullptr;
    });

    auto colorPreset
        = SettingsHandler::get_instance()->get_current_color_preset();
    const QColor& text = colorPreset[EPresetsColorIdx::kTextColor];
    const QColor& background = colorPreset[EPresetsColorIdx::kBackgroundColor];
    const QColor& border = colorPreset[EPresetsColorIdx::kBorderColor];
    const QColor& selection = colorPreset[EPresetsColorIdx::kSelectionColor];
    popup->setStyleSheet(
        QStringLiteral("QWidget {"
                       "  background-color: %1;"
                       "  color: %2;"
                       "  border: 1px solid %3;"
                       "  border-radius: 6px;"
                       "}"
                       "QToolButton {"
                       "  border: none;"
                       "  border-radius: 4px;"
                       "  padding: 4px 8px;"
                       "}"
                       "QToolButton:checked, QToolButton:hover {"
                       "  background-color: rgba(%4, %5, %6, 90);"
                       "}")
            .arg(background.name(), text.name(), border.name())
            .arg(selection.red())
            .arg(selection.green())
            .arg(selection.blue()));

    auto* grid = new QGridLayout(popup);
    grid->setContentsMargins(6, 6, 6, 6);
    grid->setSpacing(2);

    const int speeds[] = {25, 50, 75, 100, 125, 150, 175, 200};
    const int currentSpeed = item_->speed_percent();
    for (int i = 0; i < 8; ++i) {
        const int percent = speeds[i];
        auto* btn = new QToolButton(popup);
        btn->setText(QStringLiteral("x%1").arg(percent / 100.0, 0, 'g', 3));
        btn->setCheckable(true);
        btn->setChecked(percent == currentSpeed);
        btn->setFocusPolicy(Qt::NoFocus);
        grid->addWidget(btn, i / 4, i % 4);
        connect(btn, &QToolButton::clicked, this, [this, percent, popup] {
            if (item_) {
                item_->set_speed_percent(percent);
            }
            update_speed_label();
            popup->close();
        });
    }

    auto* escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), popup);
    connect(escShortcut, &QShortcut::activated, popup, &QWidget::close);

    popup->move(speedBtn_->mapToGlobal(speedBtn_->rect().bottomLeft()));
    popup->show();
}

void GifPlaybackToolbar::toggle_filmstrip()
{
    if (framesBtn_->isChecked()) {
        rebuild_filmstrip();
        filmstrip_->show();
    } else {
        filmstrip_->hide();
    }
    // The outer layout uses the default size constraint, which only
    // updates the widget's min/max size when a child's visibility
    // changes - it does NOT resize the widget itself. Without this, the
    // toolbar stays at its old (filmstrip-hidden) size and the newly
    // shown thumbnails just get clipped inside it instead of the widget
    // growing to fit them.
    adjustSize();
}

void GifPlaybackToolbar::rebuild_filmstrip()
{
    if (!item_) {
        return;
    }

    const QLayoutItem* child = nullptr;
    while ((child = filmstripLay_->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    const QList<QPixmap>& thumbs = item_->frame_thumbnails();
    const int current = item_->current_frame();
    for (int i = 0; i < thumbs.size(); ++i) {
        auto* btn = new QToolButton(filmstrip_);
        btn->setIcon(QIcon(thumbs.at(i)));
        btn->setIconSize(QSize(kthumbSize, kthumbSize));
        btn->setText(QString::number(i + 1));
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setCheckable(true);
        btn->setChecked(i == current);
        btn->setAutoRaise(true);
        btn->setFocusPolicy(Qt::NoFocus);
        connect(btn, &QToolButton::clicked, this, [this, i] {
            if (item_) {
                item_->jump_to_frame(i);
            }
        });
        filmstripLay_->addWidget(btn);
    }
    filmstripLay_->addStretch();
}

void GifPlaybackToolbar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    emit geometry_changed();
}

void GifPlaybackToolbar::position_controls_row(int desiredCenterX)
{
    int x = desiredCenterX - (controlsRow_->width() / 2);
    x = qBound(0, x, qMax(0, width() - controlsRow_->width()));
    controlsRow_->move(x, controlsRow_->y());
}

void GifPlaybackToolbar::restyle_from_preset()
{
    auto colorPreset
        = SettingsHandler::get_instance()->get_current_color_preset();
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

    setStyleSheet(QStringLiteral("GifPlaybackToolbar > QWidget {"
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

    const qreal dpr = devicePixelRatioF();
    prevBtn_->setIcon(make_step_icon(iconGlyphColor_, dpr, /*prev=*/true));
    nextBtn_->setIcon(make_step_icon(iconGlyphColor_, dpr, /*prev=*/false));
    framesBtn_->setIcon(make_filmstrip_icon(iconGlyphColor_, dpr));
    update_play_pause_icon();
}
