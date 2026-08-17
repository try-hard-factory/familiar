#include "group_toolbar.h"
#include "widgets/color_picker_dialog.h"

#include <core/settingshandler.h>
#include <moveitem.h>

#include <QCheckBox>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QResizeEvent>
#include <QShortcut>
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

// Fill-color button icon: just a rounded-rect swatch of the group's
// current fill, bordered so it reads clearly even against a similarly
// dark fill - no letter glyph needed (unlike TextEditToolbar's B/H/BG
// buttons, this is the ONLY color control on this bar, nothing to
// disambiguate).
QIcon makeFillColorIcon(const QColor& fillColor,
                        const QColor& borderColor,
                        qreal dpr)
{
    QPixmap pm(QSize(kIconSize, kIconSize) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    QPen border(borderColor);
    border.setWidthF(1.2);
    p.setPen(border);
    p.setBrush(fillColor);
    p.drawRoundedRect(QRectF(1.5, 1.5, kIconSize - 3.0, kIconSize - 3.0),
                      3.0,
                      3.0);

    p.end();
    QIcon icon;
    icon.addPixmap(pm);
    return icon;
}

// Small downward chevron - opens showSettingsPopup_(). Same drawn-icon
// approach as every other button on this bar, no external asset.
QIcon makeChevronIcon(const QColor& glyphColor, qreal dpr)
{
    QPixmap pm(QSize(kIconSize, kIconSize) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    QPen pen(glyphColor);
    pen.setWidthF(2.0);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    QPainterPath chevron;
    chevron.moveTo(kIconSize * 0.28, kIconSize * 0.4);
    chevron.lineTo(kIconSize * 0.5, kIconSize * 0.62);
    chevron.lineTo(kIconSize * 0.72, kIconSize * 0.4);
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

    fillColorBtn_ = new QToolButton(row);
    fillColorBtn_->setToolTip(tr("Group fill color"));
    fillColorBtn_->setAutoRaise(true);
    fillColorBtn_->setFixedSize(kButtonSize, kButtonSize);
    fillColorBtn_->setFocusPolicy(Qt::NoFocus);
    lay->addWidget(fillColorBtn_);

    chevronBtn_ = new QToolButton(row);
    chevronBtn_->setToolTip(tr("Group settings"));
    chevronBtn_->setAutoRaise(true);
    chevronBtn_->setFixedSize(kButtonSize, kButtonSize);
    chevronBtn_->setFocusPolicy(Qt::NoFocus);
    lay->addWidget(chevronBtn_);

    outer->addWidget(row);

    connect(lockBtn_,
            &QToolButton::toggled,
            this,
            &GroupToolbar::onLockToggled_);
    connect(fillColorBtn_, &QToolButton::clicked, this, [this] {
        if (!item_)
            return;
        const QColor oldColor = item_->fill_color();
        QColor initial = oldColor;
        initial.setAlpha(255);
        // Live preview while dragging - applies straight to the item,
        // bypassing the undo stack, exactly like the item
        // would look if the color really were already committed;
        // reverted below on cancel, or folded into ONE real undo
        // command on accept.
        ColorPickerDialog dialog(this, initial, tr("Group fill color"));
        connect(&dialog,
                &ColorPickerDialog::colorChanged,
                this,
                [this](QColor c) {
                    if (item_) {
                        item_->set_fill_color(c);
                        updateFillColorIcon_();
                    }
                });
        if (dialog.exec() == QDialog::Accepted) {
            const QColor color = dialog.selectedColor();
            if (auto* scene = dynamic_cast<CanvasScene*>(item_->scene())) {
                // Undo the live preview first so the undo command's own
                // redo() (which runs immediately on push()) is the only
                // thing that actually sets fill_color to `color`.
                item_->set_fill_color(oldColor);
                scene->undo_stack_->push(
                    new ChangeGroupFillColorCommand(item_, color, oldColor));
            } else {
                item_->set_fill_color(color);
            }
        } else {
            item_->set_fill_color(oldColor);
        }
        updateFillColorIcon_();
    });
    connect(chevronBtn_,
            &QToolButton::clicked,
            this,
            &GroupToolbar::showSettingsPopup_);

    restyleFromPreset();
}

void GroupToolbar::attach(GroupItem* item)
{
    item_ = item;
    if (item_) {
        const QSignalBlocker blocker(lockBtn_);
        lockBtn_->setChecked(item_->locked());
    }
    updateFillColorIcon_();
}

void GroupToolbar::onLockToggled_(bool checked)
{
    if (item_)
        item_->set_locked(checked);
}

void GroupToolbar::updateFillColorIcon_()
{
    if (!item_)
        return;
    fillColorBtn_->setIcon(makeFillColorIcon(item_->fill_color(),
                                             iconGlyphColor_,
                                             devicePixelRatioF()));
}

void GroupToolbar::showSettingsPopup_()
{
    if (!item_)
        return;

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

    auto colorPreset = SettingsHandler::getInstance()->getCurrentColorPreset();
    const QColor& text = colorPreset[EPresetsColorIdx::kTextColor];
    const QColor& background = colorPreset[EPresetsColorIdx::kBackgroundColor];
    const QColor& border = colorPreset[EPresetsColorIdx::kBorderColor];
    popup->setStyleSheet(
        QStringLiteral("QWidget {"
                       "  background-color: %1;"
                       "  color: %2;"
                       "  border: 1px solid %3;"
                       "  border-radius: 6px;"
                       "}")
            .arg(background.name(), text.name(), border.name()));

    auto* lay = new QVBoxLayout(popup);
    lay->setContentsMargins(10, 8, 10, 8);

    auto* dragDropCheck = new QCheckBox(tr("Drag and drop items into group"),
                                        popup);
    dragDropCheck->setChecked(item_->drag_drop_enabled());
    dragDropCheck->setFocusPolicy(Qt::NoFocus);
    connect(dragDropCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (item_)
            item_->set_drag_drop_enabled(checked);
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
                      .arg(rgba(background, 245),
                           border.name(),
                           text.name(),
                           rgba(selection, 90),
                           rgba(selection, 170),
                           background.name()));

    lockBtn_->setIcon(makeLockIcon(iconGlyphColor_, devicePixelRatioF()));
    chevronBtn_->setIcon(makeChevronIcon(iconGlyphColor_, devicePixelRatioF()));
    updateFillColorIcon_();
}
