#include "colors_widget.h"
#include <core/settingshandler.h>

#include "log/log.h"
using namespace familiar::log;
#include "widgets/dialogs.h"
#include <ui/extendedslider.h>
#include <ui/presetsave_window.h>
#include <utils/utils.h>
#include <widgets/color_picker_dialog.h>
#include <widgets/settings_style.h>

#include <QAbstractButton>
#include <QButtonGroup>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kswatchDiameter = 30;

// Fixed mid-gray, not settings_style::palette().border (#D8D8D8) - that
// value is tuned for hairlines against this window's own fixed white
// background, too close to white to read as an outline against a swatch
// whose fill also happens to be light/white (e.g. a default "Background
// color" preset value) - the ring all but disappeared (confirmed
// visually). This is the swatch's own edge against the page, not a
// window-chrome hairline, so it gets its own darker constant instead of
// reusing that one.
const QColor kswatchBorderColor(0xB0, 0xB0, 0xB0);

QIcon make_swatch_icon(const QColor& color, qreal dpr)
{
    QPixmap pm(QSize(kswatchDiameter, kswatchDiameter) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen border(kswatchBorderColor);
    border.setWidthF(1.5);
    p.setPen(border);
    p.setBrush(color);
    p.drawEllipse(
        QRectF(1.0, 1.0, kswatchDiameter - 2.0, kswatchDiameter - 2.0));
    p.end();

    QIcon icon;
    icon.addPixmap(pm);
    return icon;
}

} // namespace


ColorsWidget::ColorsWidget(QWidget* parent)
    : QWidget(parent)
    , layout_(new QVBoxLayout(this))
    , opacitySlider_(new ExtendedSlider())
    , headerLayout_(new QHBoxLayout())
    , bodyLayout_(new QHBoxLayout())
    , sliderLayout_(new QHBoxLayout())
    , bottomLayout_(new QHBoxLayout())
{
    layout_->setAlignment(Qt::AlignTop);

    labels_init();
    presets_init();
    color_init();
    slider_init();
    save_reset_btns_init();

    layout_->addLayout(headerLayout_);
    layout_->addLayout(bodyLayout_);
    layout_->addSpacing(50);
    layout_->addLayout(sliderLayout_);
    layout_->addSpacing(50);
    layout_->addLayout(bottomLayout_);

    setLayout(layout_);
}


ColorsWidget::~ColorsWidget()
{
    delete layout_;
}

void ColorsWidget::labels_init()
{
    auto* presetsLbl = new QLabel(tr("Presets"));
    presetsLbl->setAlignment(Qt::AlignLeft);
    auto* colorsLbl = new QLabel(tr("Colors"));
    colorsLbl->setAlignment(Qt::AlignCenter);
    headerLayout_->addWidget(presetsLbl);
    headerLayout_->addWidget(colorsLbl);
}


void ColorsWidget::presets_init()
{
    auto* presetsLayout = new QVBoxLayout();
    presetsLayout->setAlignment(Qt::AlignLeft);
    presetsLayout->setSpacing(4);

    presetButtons_ = new QButtonGroup(this);
    presetButtons_->setExclusive(true);

    struct PresetBtn
    {
        QString label;
        EPresets preset;
    };
    const QList<PresetBtn> presets = {
        {tr("Dark"), EPresets::kDarkPreset},
        {tr("Light"), EPresets::kLightPreset},
        {tr("Custom 1"), EPresets::kCustom1},
        {tr("Custom 2"), EPresets::kCustom2},
        {tr("Custom 3"), EPresets::kCustom3},
        {tr("Custom 4"), EPresets::kCustom4},
    };

    for (const PresetBtn& preset : presets) {
        auto* btn = new QPushButton(preset.label, this);
        // Same #categoryButton chrome as the sidebar's own
        // CategoryNavButton (ui/settings_window.cpp): idle/hover/checked
        // fill. No custom paintEvent needed here the way that class
        // needs one - preset names never need the bold search-match
        // rich text CategoryNavButton exists for, so plain
        // QPushButton::setText() (which sidebarButtonStyleSheet() alone
        // can already color via the window's inherited "* { color }"
        // cascade) is enough.
        btn->setObjectName(QStringLiteral("categoryButton"));
        // sidebarButtonStyleSheet() alone has no padding/min-height -
        // CategoryNavButton (ui/settings_window.cpp) gets away with that
        // because it hand-paints its own text instead of relying on
        // QPushButton::setText()'s native layout, so it never needed
        // breathing room from QSS. This button does rely on native text
        // layout, so without this "Custom 1" etc. came out clipped
        // against the button edges (confirmed visually). NOT
        // CategoryNavButton's own 38px min-height though - six of these
        // stacked in this narrower column at that height ran into each
        // other with zero visible gap (confirmed visually) - same
        // min-height/padding as filledButtonStyleSheet()'s buttons
        // instead (Restore Defaults etc., which read at a normal size).
        // Also overrides sidebarButtonStyleSheet()'s own "text-align:
        // left" (matches the sidebar's icon+label list convention) -
        // this column is short standalone preset names, not a list of
        // longer category labels, so centered reads better. Same
        // selector, appended after the base sheet in this one combined
        // string - later wins on a tie, no separate stylesheet needed.
        btn->setStyleSheet(familiar::settings_style::sidebar_button_style_sheet()
                           + QStringLiteral("QPushButton#categoryButton {"
                                            "  padding: 4px 14px;"
                                            "  text-align: center;"
                                            "}"));
        btn->setMinimumHeight(22);
        btn->setMinimumWidth(110);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);

        const EPresets value = preset.preset;
        connect(btn, &QPushButton::clicked, this, [value]() {
            SettingsHandler::get_instance()->set_current_preset(value);
            emit SettingsHandler::get_instance() -> presets_changed();
        });

        presetButtons_->addButton(btn, static_cast<int>(preset.preset));
        presetsLayout->addWidget(btn);
    }

    if (QAbstractButton* checked = presetButtons_->button(
            SettingsHandler::get_instance()->current_preset())) {
        checked->setChecked(true);
    }

    connect(SettingsHandler::get_instance(),
            &SettingsHandler::presets_changed,
            this,
            &ColorsWidget::update_components);

    bodyLayout_->addLayout(presetsLayout);
}


void ColorsWidget::color_init()
{
    auto* colorsLayout = new QVBoxLayout();
    colorsLayout->setAlignment(Qt::AlignRight);

    struct ColorRow
    {
        QString label;
        EPresetsColorIdx idx;
    };
    const QList<ColorRow> rows = {
        {tr("Background color: "), EPresetsColorIdx::kBackgroundColor},
        {tr("Canvas color: "), EPresetsColorIdx::kCanvasColor},
        {tr("Border color: "), EPresetsColorIdx::kBorderColor},
        {tr("UI Text Color: "), EPresetsColorIdx::kTextColor},
        {tr("Selection color: "), EPresetsColorIdx::kSelectionColor},
    };

    for (const ColorRow& row : rows) {
        auto* rowLayout = new QHBoxLayout();
        rowLayout->setAlignment(Qt::AlignRight);

        auto* colorLbl = new QLabel(row.label);
        colorLbl->setAlignment(Qt::AlignRight);
        rowLayout->addWidget(colorLbl);

        auto* swatch = new QToolButton(this);
        swatch->setFixedSize(kswatchDiameter, kswatchDiameter);
        swatch->setAutoRaise(true);
        swatch->setCursor(Qt::PointingHandCursor);
        swatch->setStyleSheet(QStringLiteral(
            "QToolButton { border: none; background: transparent; }"));
        rowLayout->addWidget(swatch);

        colorSwatches_[row.idx] = swatch;
        const EPresetsColorIdx idx = row.idx;
        connect(swatch, &QToolButton::clicked, this, [this, idx]() {
            pick_color(idx);
        });

        colorsLayout->addLayout(rowLayout);
    }

    for (int i = 0; i < EPresetsColorIdx::kAllIdx; ++i) {
        refresh_swatch(static_cast<EPresetsColorIdx>(i));
    }

    bodyLayout_->addLayout(colorsLayout);
}

void ColorsWidget::pick_color(EPresetsColorIdx idx)
{
    auto* settings = SettingsHandler::get_instance();
    const auto preset = settings->get_current_color_preset();
    const QColor oldColor = preset[idx];

    const QMap<EPresetsColorIdx, QString> titles = {
        {EPresetsColorIdx::kBackgroundColor, tr("Background color")},
        {EPresetsColorIdx::kCanvasColor, tr("Canvas color")},
        {EPresetsColorIdx::kBorderColor, tr("Border color")},
        {EPresetsColorIdx::kTextColor, tr("UI Text Color")},
        {EPresetsColorIdx::kSelectionColor, tr("Selection color")},
    };

    ColorPickerDialog dialog(this, oldColor, titles.value(idx));
    connect(&dialog,
            &ColorPickerDialog::color_changed,
            this,
            [this, settings, idx](QColor c) {
                auto p = settings->get_current_color_preset();
                p[idx] = c;
                settings->set_current_color_preset(p);
                refresh_swatch(idx);
                emit SettingsHandler::get_instance() -> settings_changed();
            });

    if (dialog.exec() != QDialog::Accepted) {
        // Cancelled - revert the live preview colorChanged() applied
        // above while dragging, same convention GroupToolbar's fill-color
        // button uses (ui/group_toolbar.cpp).
        auto p = settings->get_current_color_preset();
        p[idx] = oldColor;
        settings->set_current_color_preset(p);
        refresh_swatch(idx);
        emit SettingsHandler::get_instance() -> settings_changed();
    }
}

void ColorsWidget::refresh_swatch(EPresetsColorIdx idx)
{
    QToolButton* swatch = colorSwatches_[idx];
    if (!swatch) {
        return;
    }
    const auto preset = SettingsHandler::get_instance()->get_current_color_preset();
    swatch->setIcon(make_swatch_icon(preset[idx], devicePixelRatioF()));
}


void ColorsWidget::slider_init()
{
    // TODOLATER
    auto* settings = SettingsHandler::get_instance();
    // slider init
    opacitySlider_->setFocusPolicy(Qt::NoFocus);
    opacitySlider_->setOrientation(Qt::Horizontal);
    opacitySlider_->setRange(0, 100);
    opacitySlider_->setCursor(Qt::PointingHandCursor);
    opacitySlider_->setStyleSheet(familiar::settings_style::slider_style_sheet());
    sliderLayout_->setAlignment(Qt::AlignBottom);
    sliderLayout_->addWidget(new QLabel(QStringLiteral("Master opacity:")));
    sliderLayout_->addWidget(opacitySlider_);

    opacitySlider_->set_maped_value(0, settings->get_current_opacity(), 255);
    connect(opacitySlider_, &ExtendedSlider::valueChanged, [this]() {
        FLOG_DEBUG(Ch::UI,
                   "Master opacity from settings = {}",
                   debug_string(SettingsHandler::get_instance()->master_opacity()));
        SettingsHandler::get_instance()->set_current_opacity(
            opacitySlider_->mapped_value(0, 255));
        //qDebug()<<"Opacity: "<<opacitySlider_->mappedValue(0, 255);
        emit SettingsHandler::get_instance() -> settings_changed();
    });
}


void ColorsWidget::save_reset_btns_init()
{
    // Same filled-gray-box chrome as the window's own bottom row
    // (Restore Defaults/Import/Export, ui/settings_window.cpp) - not
    // dialog_style::styleSecondaryButton()'s outline look, which is for
    // separate modal dialogs, not buttons living inside this window.
    QPushButton* saveToPresetBtn = new QPushButton(tr("Save to preset"),
                                                      this);
    saveToPresetBtn->setStyleSheet(
        familiar::settings_style::filled_button_style_sheet());
    connect(saveToPresetBtn,
            &QPushButton::clicked,
            this,
            &ColorsWidget::show_preset_save_window);

    // "Reset to default" used to live here too - redundant now that the
    // window's own Restore Defaults button (settings_window.cpp) opens
    // RestoreDefaultsDialog with a Colors category covering exactly the
    // same reset, per-category checkbox and all. Max's call.
    bottomLayout_->addWidget(saveToPresetBtn);
}

void ColorsWidget::show_preset_save_window()
{
    PresetSaveWindow* widget = new PresetSaveWindow(parentWidget());
    widget->show();
    centered_widget(this, widget);
}


void ColorsWidget::update_components()
{
    auto* settings = SettingsHandler::get_instance();

    for (int i = 0; i < EPresetsColorIdx::kAllIdx; ++i) {
        refresh_swatch(static_cast<EPresetsColorIdx>(i));
    }

    opacitySlider_->set_maped_value(0, settings->get_current_opacity(), 255);

    if (QAbstractButton* checked = presetButtons_->button(
            settings->current_preset())) {
        checked->setChecked(true);
    }

    emit SettingsHandler::get_instance() -> settings_changed();
}
