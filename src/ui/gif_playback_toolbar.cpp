#include "gif_playback_toolbar.h"
#include "widgets/dialog_style.h"

#include <array>

#include <core/settingshandler.h>
#include <moveitem.h>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLayoutItem>
#include <QList>
#include <QMargins>
#include <QMovie>
#include <QObject>
#include <QPaintDevice>
#include <QPainter>
#include <QPixmap>
#include <QPolygon>
#include <QResizeEvent>
#include <QShortcut>
#include <QToolButton>
#include <QVBoxLayout>
#include <Qt>
#include <QtMinMax>
#include <QtPreprocessorSupport>
#include <QtTypes>

namespace {

constexpr int kbuttonSize = 30;
constexpr int kiconSize = 18;
constexpr int kthumbSize = 48;

// Every glyph on this bar is drawn inside a kiconSize square with the same
// inset from the edges, so prev/play/next/pause line up optically.
constexpr qreal kglyphInsetX = 4;
constexpr qreal kglyphInsetY = 3;
// The step glyphs (prev/next) are a triangle plus a thin bar against one
// edge, so their triangle sits one pixel further in to leave room.
constexpr qreal kstepTriangleInset = 5;
constexpr qreal kstepTriangleTip = 7;
constexpr qreal kstepBarWidth = 2;
// Pause: two bars of this width, inset from either edge.
constexpr qreal kpauseBarWidth = 4;
// Filmstrip: three outlined cells across, with a gap between them.
constexpr qreal kfilmstripStroke = 1.6;
constexpr int kfilmstripCells = 3;
constexpr qreal kfilmstripInset = 3;
constexpr qreal kfilmstripTop = 4;
constexpr qreal kfilmstripGap = 2;

// Alphas for the popup's own QSS - panel, hover, pressed.
// Layout of the bar itself.
constexpr int kouterSpacing = 6;
constexpr int kspeedLabelPadding = 16;

constexpr int kpopupBgAlpha = 245;
constexpr int khoverAlpha = 90;
constexpr int kpressedAlpha = 170;

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

    const qreal iconMidY = kiconSize / 2.0;
    const qreal farX = kiconSize - kglyphInsetX;
    const qreal lowY = kiconSize - kglyphInsetY;

    QPolygonF tri;
    if (pointLeft) {
        tri << QPointF(farX, kglyphInsetY) << QPointF(farX, lowY)
            << QPointF(kglyphInsetX, iconMidY);
    } else {
        tri << QPointF(kglyphInsetX, kglyphInsetY)
            << QPointF(kglyphInsetX, lowY) << QPointF(farX, iconMidY);
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

    const qreal iconMidY = kiconSize / 2.0;
    const qreal glyphHeight = kiconSize - (2 * kglyphInsetY);

    QPolygonF tri;
    QRectF bar;
    if (prev) {
        const qreal baseX = kiconSize - kstepTriangleInset;
        tri << QPointF(baseX, kglyphInsetY)
            << QPointF(baseX, kiconSize - kglyphInsetY)
            << QPointF(kstepTriangleTip, iconMidY);
        bar = QRectF(kfilmstripInset, kglyphInsetY, kstepBarWidth, glyphHeight);
    } else {
        tri << QPointF(kstepTriangleInset, kglyphInsetY)
            << QPointF(kstepTriangleInset, kiconSize - kglyphInsetY)
            << QPointF(kiconSize - kstepTriangleTip, iconMidY);
        bar = QRectF(kiconSize - kstepTriangleInset,
                     kglyphInsetY,
                     kstepBarWidth,
                     glyphHeight);
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
    const qreal barHeight = kiconSize - (2 * kglyphInsetY);
    p.drawRect(QRectF(kglyphInsetX, kglyphInsetY, kpauseBarWidth, barHeight));
    p.drawRect(QRectF(kiconSize - kglyphInsetX - kpauseBarWidth,
                      kglyphInsetY,
                      kpauseBarWidth,
                      barHeight));

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
    pen.setWidthF(kfilmstripStroke);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    const qreal cellW = (kiconSize - (2 * kfilmstripInset))
                        / qreal(kfilmstripCells);
    const qreal cellH = kiconSize - (2 * kfilmstripTop);
    for (int i = 0; i < kfilmstripCells; ++i) {
        p.drawRect(QRectF(kfilmstripInset + (i * cellW),
                          kfilmstripTop,
                          cellW - kfilmstripGap,
                          cellH));
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

    familiar::dialog_style::apply_toolbar_shadow(this);


    outerLay_->setContentsMargins(0, 0, 0, 0);
    outerLay_->setSpacing(kouterSpacing);


    auto* row = controlsRow_;
    auto* lay = new QHBoxLayout(row);
    constexpr QMargins kbarMargins{8, 5, 8, 5};
    lay->setContentsMargins(kbarMargins);
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
                                + kspeedLabelPadding,
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


    constexpr QMargins kfilmstripMargins{6, 4, 6, 6};
    filmstripLay_->setContentsMargins(kfilmstripMargins);
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
    const QColor& text = colorPreset[EPresetsColorIdx::TextColor];
    const QColor& background = colorPreset[EPresetsColorIdx::BackgroundColor];
    const QColor& border = colorPreset[EPresetsColorIdx::BorderColor];
    const QColor& selection = colorPreset[EPresetsColorIdx::SelectionColor];
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
    constexpr QMargins kspeedGridMargins{6, 6, 6, 6};
    grid->setContentsMargins(kspeedGridMargins);
    grid->setSpacing(2);

    constexpr std::array<int, 8> kspeedPercents
        = {25, 50, 75, 100, 125, 150, 175, 200};
    constexpr int kspeedColumns = 4;
    const int currentSpeed = item_->speed_percent();
    // Explicit counter rather than indexing: the loop bound used to be a
    // hand-written 8 with no tie to the array's length, so adding a speed
    // would have silently skipped it (or read past the end).
    int index = 0;
    for (const int percent : kspeedPercents) {
        auto* btn = new QToolButton(popup);
        btn->setText(QStringLiteral("x%1").arg(percent / 100.0, 0, 'g', 3));
        btn->setCheckable(true);
        btn->setChecked(percent == currentSpeed);
        btn->setFocusPolicy(Qt::NoFocus);
        grid->addWidget(btn, index / kspeedColumns, index % kspeedColumns);
        connect(btn, &QToolButton::clicked, this, [this, percent, popup] {
            if (item_) {
                item_->set_speed_percent(percent);
            }
            update_speed_label();
            popup->close();
        });
        ++index;
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
    const QColor& text = colorPreset[EPresetsColorIdx::TextColor];
    const QColor& background = colorPreset[EPresetsColorIdx::BackgroundColor];
    const QColor& border = colorPreset[EPresetsColorIdx::BorderColor];
    const QColor& selection = colorPreset[EPresetsColorIdx::SelectionColor];
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
                      .arg(rgba(background, kpopupBgAlpha),
                           border.name(),
                           text.name(),
                           rgba(selection, khoverAlpha),
                           rgba(selection, kpressedAlpha),
                           background.name()));

    const qreal dpr = devicePixelRatioF();
    prevBtn_->setIcon(make_step_icon(iconGlyphColor_, dpr, /*prev=*/true));
    nextBtn_->setIcon(make_step_icon(iconGlyphColor_, dpr, /*prev=*/false));
    framesBtn_->setIcon(make_filmstrip_icon(iconGlyphColor_, dpr));
    update_play_pause_icon();
}
