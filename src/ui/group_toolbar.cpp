#include "group_toolbar.h"

#include "canvasscene.h"
#include "commands.h"
#include "widgets/color_picker_dialog.h"
#include "widgets/dialog_style.h"

#include <core/settingshandler.h>
#include <moveitem.h>

#include <QCheckBox>
#include <QDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QMargins>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QResizeEvent>
#include <QShortcut>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>
#include <Qt>
#include <QtTypes>

namespace {

constexpr int kbuttonSize = 30;
constexpr int kiconSize = 18;

// Stroke widths shared by the drawn glyphs on this bar.
constexpr qreal kglyphStroke = 2.0;
constexpr qreal kswatchBorderStroke = 1.2;
// Corner radii.
constexpr qreal klockBodyRadius = 2.0;
constexpr qreal kswatchRadius = 3.0;
// A half-turn, for the padlock shackle's arc.
constexpr int khalfTurnDegrees = 180;

// The padlock is laid out as fractions of kiconSize so it scales with the
// icon rather than being pinned to 18px.
constexpr qreal kshackleX = 0.28;
constexpr qreal kshackleY = 0.12;
constexpr qreal kshackleSize = 0.44;
constexpr qreal klockBodyX = 0.18;
constexpr qreal klockBodyY = 0.46;
constexpr qreal klockBodyW = 0.64;
constexpr qreal klockBodyH = 0.42;

// Colour swatch: inset so its border is not clipped by the pixmap edge.
constexpr qreal kswatchInset = 1.5;

// Chevron, same fraction-of-icon idea.
constexpr qreal kchevronLeft = 0.28;
constexpr qreal kchevronMid = 0.5;
constexpr qreal kchevronRight = 0.72;
constexpr qreal kchevronTop = 0.4;
constexpr qreal kchevronBottom = 0.62;

// Alphas for the popup's own QSS - panel, hover, pressed.
constexpr int kpopupBgAlpha = 245;
constexpr int khoverAlpha = 90;
constexpr int kpressedAlpha = 170;

// Simple drawn padlock glyph - shackle (arc) + body (rounded rect), same
// QPainter-drawn-icon approach as gif_playback_toolbar.cpp's makeStepIcon()
// etc.
QIcon make_lock_icon(const QColor& glyphColor, qreal dpr)
{
    QPixmap pm(QSize(kiconSize, kiconSize) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    QPen shacklePen(glyphColor);
    shacklePen.setWidthF(kglyphStroke);
    shacklePen.setCapStyle(Qt::RoundCap);
    p.setPen(shacklePen);
    p.setBrush(Qt::NoBrush);
    QPainterPath shackle;
    const QRectF shackleRect(kiconSize * kshackleX,
                             kiconSize * kshackleY,
                             kiconSize * kshackleSize,
                             kiconSize * kshackleSize);
    shackle.arcMoveTo(shackleRect, 0);
    shackle.arcTo(shackleRect, 0, khalfTurnDegrees);
    p.drawPath(shackle);

    p.setPen(Qt::NoPen);
    p.setBrush(glyphColor);
    p.drawRoundedRect(QRectF(kiconSize * klockBodyX,
                             kiconSize * klockBodyY,
                             kiconSize * klockBodyW,
                             kiconSize * klockBodyH),
                      klockBodyRadius,
                      klockBodyRadius);

    p.end();
    QIcon icon;
    icon.addPixmap(pm);
    return icon;
}

// Fill-color button icon: just a rounded-rect swatch of the group's
// current fill, bordered so it reads clearly even against a similarly
// dark fill - no letter glyph needed (unlike TextEditToolbar's B/H/BG
// buttons, this is the ONLY color control on this bar, nothing to
// disambiguate).
QIcon make_fill_color_icon(const QColor& fillColor,
                           const QColor& borderColor,
                           qreal dpr)
{
    QPixmap pm(QSize(kiconSize, kiconSize) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    QPen border(borderColor);
    border.setWidthF(kswatchBorderStroke);
    p.setPen(border);
    p.setBrush(fillColor);
    const qreal swatchSide = kiconSize - (2 * kswatchInset);
    p.drawRoundedRect(QRectF(kswatchInset, kswatchInset, swatchSide, swatchSide),
                      kswatchRadius,
                      kswatchRadius);

    p.end();
    QIcon icon;
    icon.addPixmap(pm);
    return icon;
}

// Small downward chevron - opens showSettingsPopup_(). Same drawn-icon
// approach as every other button on this bar, no external asset.
QIcon make_chevron_icon(const QColor& glyphColor, qreal dpr)
{
    QPixmap pm(QSize(kiconSize, kiconSize) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    QPen pen(glyphColor);
    pen.setWidthF(kglyphStroke);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    QPainterPath chevron;
    chevron.moveTo(kiconSize * kchevronLeft, kiconSize * kchevronTop);
    chevron.lineTo(kiconSize * kchevronMid, kiconSize * kchevronBottom);
    chevron.lineTo(kiconSize * kchevronRight, kiconSize * kchevronTop);
    p.drawPath(chevron);

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

    familiar::dialog_style::apply_toolbar_shadow(this);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* row = new QWidget(this);
    auto* lay = new QHBoxLayout(row);
    constexpr QMargins kbarMargins{8, 5, 8, 5};
    lay->setContentsMargins(kbarMargins);
    lay->setSpacing(2);

    lockBtn_ = new QToolButton(row);
    lockBtn_->setToolTip(tr("Lock group"));
    lockBtn_->setCheckable(true);
    lockBtn_->setAutoRaise(true);
    lockBtn_->setFixedSize(kbuttonSize, kbuttonSize);
    lockBtn_->setFocusPolicy(Qt::NoFocus);
    lay->addWidget(lockBtn_);

    fillColorBtn_ = new QToolButton(row);
    fillColorBtn_->setToolTip(tr("Group fill color"));
    fillColorBtn_->setAutoRaise(true);
    fillColorBtn_->setFixedSize(kbuttonSize, kbuttonSize);
    fillColorBtn_->setFocusPolicy(Qt::NoFocus);
    lay->addWidget(fillColorBtn_);

    chevronBtn_ = new QToolButton(row);
    chevronBtn_->setToolTip(tr("Group settings"));
    chevronBtn_->setAutoRaise(true);
    chevronBtn_->setFixedSize(kbuttonSize, kbuttonSize);
    chevronBtn_->setFocusPolicy(Qt::NoFocus);
    lay->addWidget(chevronBtn_);

    outer->addWidget(row);

    connect(lockBtn_,
            &QToolButton::toggled,
            this,
            &GroupToolbar::on_lock_toggled);
    connect(fillColorBtn_, &QToolButton::clicked, this, [this] {
        if (!item_) {
            return;
        }
        const QColor oldColor = item_->fill_color();
        QColor initial = oldColor;
        initial.setAlpha(kfullOpacity);
        // Live preview while dragging - applies straight to the item,
        // bypassing the undo stack, exactly like the item
        // would look if the color really were already committed;
        // reverted below on cancel, or folded into ONE real undo
        // command on accept.
        ColorPickerDialog dialog(this, initial, tr("Group fill color"));
        connect(&dialog,
                &ColorPickerDialog::color_changed,
                this,
                [this](QColor c) {
                    if (item_) {
                        item_->set_fill_color(c);
                        update_fill_color_icon();
                    }
                });
        if (dialog.exec() == QDialog::Accepted) {
            const QColor color = dialog.selected_color();
            if (auto* scene = dynamic_cast<CanvasScene*>(item_->scene())) {
                // Undo the live preview first so the undo command's own
                // redo() (which runs immediately on push()) is the only
                // thing that actually sets fill_color to `color`.
                item_->set_fill_color(oldColor);
                scene->undo_stack()->push(
                    new ChangeGroupFillColorCommand(item_, color, oldColor));
            } else {
                item_->set_fill_color(color);
            }
        } else {
            item_->set_fill_color(oldColor);
        }
        update_fill_color_icon();
    });
    connect(chevronBtn_,
            &QToolButton::clicked,
            this,
            &GroupToolbar::show_settings_popup);

    restyle_from_preset();
}

void GroupToolbar::attach(GroupItem* item)
{
    item_ = item;
    if (item_) {
        const QSignalBlocker blocker(lockBtn_);
        lockBtn_->setChecked(item_->locked());
    }
    update_fill_color_icon();
}

void GroupToolbar::on_lock_toggled(bool checked)
{
    if (item_) {
        item_->set_locked(checked);
    }
}

void GroupToolbar::update_fill_color_icon()
{
    if (!item_) {
        return;
    }
    fillColorBtn_->setIcon(make_fill_color_icon(item_->fill_color(),
                                                iconGlyphColor_,
                                                devicePixelRatioF()));
}

void GroupToolbar::show_settings_popup()
{
    if (!item_) {
        return;
    }

    // Toggle, not just reuse-and-raise like GifPlaybackToolbar's speed
    // popup - clicking the chevron again while the popup is already open
    // should close it, not just refocus it. close() triggers
    // WA_DeleteOnClose, whose destroyed() connection below resets
    // settingsPopup_ to nullptr.
    if (settingsPopup_) {
        settingsPopup_->close();
        return;
    }

    // Qt::Popup, NOT Qt::Tool like GifPlaybackToolbar::showSpeedPopup_()/
    // TextEditToolbar::showLinkPopup() use - those avoid Qt::Popup
    // because it auto-closes (and, with WA_DeleteOnClose, destroys)
    // itself the instant it loses activation, which breaks if the popup
    // ever opens a further NESTED dialog of its own. This popup never
    // does (just a checkbox), so that caveat doesn't apply, and Qt::Popup
    // gives click-outside-to-dismiss for free - a manual WindowDeactivate
    // eventFilter was tried first and didn't actually fire reliably
    // (Qt::Tool windows don't get independent activation from every
    // window manager - confirmed it silently did nothing).
    auto* popup = new QWidget(nullptr, Qt::Popup);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setAttribute(Qt::WA_TranslucentBackground, false);
    settingsPopup_ = popup;
    connect(popup, &QObject::destroyed, this, [this] {
        this->settingsPopup_ = nullptr;
    });

    auto colorPreset
        = SettingsHandler::get_instance()->get_current_color_preset();
    const QColor& text = colorPreset[EPresetsColorIdx::TextColor];
    const QColor& background = colorPreset[EPresetsColorIdx::BackgroundColor];
    const QColor& border = colorPreset[EPresetsColorIdx::BorderColor];
    popup->setStyleSheet(
        QStringLiteral("QWidget {"
                       "  background-color: %1;"
                       "  color: %2;"
                       "  border: 1px solid %3;"
                       "  border-radius: 6px;"
                       "}")
            .arg(background.name(), text.name(), border.name()));

    auto* lay = new QVBoxLayout(popup);
    constexpr QMargins kpopupMargins{10, 8, 10, 8};
    lay->setContentsMargins(kpopupMargins);

    auto* dragDropCheck = new QCheckBox(tr("Drag and drop items into group"),
                                        popup);
    dragDropCheck->setChecked(item_->drag_drop_enabled());
    dragDropCheck->setFocusPolicy(Qt::NoFocus);
    connect(dragDropCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (item_) {
            item_->set_drag_drop_enabled(checked);
        }
    });
    lay->addWidget(dragDropCheck);

    // Qt::Popup already closes on Escape natively, but an explicit
    // shortcut doesn't hurt and matches every other popup in this app.
    auto* escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), popup);
    connect(escShortcut, &QShortcut::activated, popup, &QWidget::close);

    popup->move(chevronBtn_->mapToGlobal(chevronBtn_->rect().bottomLeft()));
    popup->show();
}

void GroupToolbar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    emit geometry_changed();
}

void GroupToolbar::restyle_from_preset()
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

    setStyleSheet(QStringLiteral("GroupToolbar > QWidget {"
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

    lockBtn_->setIcon(make_lock_icon(iconGlyphColor_, devicePixelRatioF()));
    chevronBtn_->setIcon(
        make_chevron_icon(iconGlyphColor_, devicePixelRatioF()));
    update_fill_color_icon();
}
