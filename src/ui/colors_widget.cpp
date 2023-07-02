#include "colors_widget.h"
#include <core/settingshandler.h>
#include <ui/extendedslider.h>
#include <ui/presetsave_window.h>
#include <utils/utils.h>
#include <QButtonGroup>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>


ColorsWidget::ColorsWidget(QWidget* parent)
    : QWidget(parent)
    , layout_(new QVBoxLayout(this))
    , opacitySlider_(new ExtendedSlider())
    , header_layout_(new QHBoxLayout())
    , body_layout_(new QHBoxLayout())
    , slider_layout_(new QHBoxLayout())
    , bottom_layout_(new QHBoxLayout())
{
    layout_->setAlignment(Qt::AlignTop);

    labelsInit();
    presetsInit();
    colorInit();
    sliderInit();
    saveResetBtnsInit();

    layout_->addLayout(header_layout_);
    layout_->addLayout(body_layout_);
    layout_->addSpacing(50);
    layout_->addLayout(slider_layout_);
    layout_->addSpacing(50);
    layout_->addLayout(bottom_layout_);

    setLayout(layout_);
}


ColorsWidget::~ColorsWidget()
{
    delete layout_;
}

void ColorsWidget::resetCurrentPreset()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this,
                                  tr("Confirm Reset"),
                                  tr("Are you sure you want to reset the configuration?"),
                                  QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        auto* settings = SettingsHandler::getInstance();
        settings->setDefaultCurrentPreset();
        //_updateComponents(true);
        emit SettingsHandler::getInstance()->presetsChanged();
    }
}


void ColorsWidget::labelsInit()
{
    auto* presets_lbl = new QLabel(tr("Presets"));
    presets_lbl->setAlignment(Qt::AlignLeft);
    auto* colors_lbl = new QLabel(tr("Colors"));
    colors_lbl->setAlignment(Qt::AlignCenter);
    header_layout_->addWidget(presets_lbl);
    header_layout_->addWidget(colors_lbl);
}


void ColorsWidget::presetsInit()
{
    // presets init
    auto* presets_layout = new QVBoxLayout();
    presets_layout->setAlignment(Qt::AlignLeft);
    QPushButton* dark_btn = new QPushButton("Dark");
    connect(dark_btn, &QPushButton::clicked, this, [this]() {
        SettingsHandler::getInstance()->setCurrentPreset(EPresets::kDarkPreset);
        emit SettingsHandler::getInstance()->presetsChanged();
    });
    QPushButton* light_btn = new QPushButton("Light");
    connect(light_btn, &QPushButton::clicked, this, [this]() {
        SettingsHandler::getInstance()->setCurrentPreset(EPresets::kLightPreset);
        emit SettingsHandler::getInstance()->presetsChanged();
    });
    QPushButton* custom1_btn = new QPushButton("Custom 1");
    connect(custom1_btn, &QPushButton::clicked, this, [this]() {
        SettingsHandler::getInstance()->setCurrentPreset(EPresets::kCustom1);
        emit SettingsHandler::getInstance()->presetsChanged();
    });
    QPushButton* custom2_btn = new QPushButton("Custom 2");
    connect(custom2_btn, &QPushButton::clicked, this, [this]() {
        SettingsHandler::getInstance()->setCurrentPreset(EPresets::kCustom2);
        emit SettingsHandler::getInstance()->presetsChanged();
    });
    QPushButton* custom3_btn = new QPushButton("Custom 3");
    connect(custom3_btn, &QPushButton::clicked, this, [this]() {
        SettingsHandler::getInstance()->setCurrentPreset(EPresets::kCustom3);
        emit SettingsHandler::getInstance()->presetsChanged();
    });
    QPushButton* custom4_btn = new QPushButton("Custom 4");
    connect(custom4_btn, &QPushButton::clicked, this, [this]() {
        SettingsHandler::getInstance()->setCurrentPreset(EPresets::kCustom4);
        emit SettingsHandler::getInstance()->presetsChanged();
    });

    connect(SettingsHandler::getInstance(),
            &SettingsHandler::presetsChanged,
            this,
            &ColorsWidget::updateComponents);

    presets_layout->addWidget(dark_btn);
    presets_layout->addWidget(light_btn);
    presets_layout->addWidget(custom1_btn);
    presets_layout->addWidget(custom2_btn);
    presets_layout->addWidget(custom3_btn);
    presets_layout->addWidget(custom4_btn);
    body_layout_->addLayout(presets_layout);
}


void ColorsWidget::colorInit()
{
    // colors init
    auto* settings = SettingsHandler::getInstance();
    auto current_preset = settings->getCurrentColorPreset();
    for (auto& color : current_preset) {
        qDebug() << color;
    }
    auto* colors_layout = new QVBoxLayout();
    colors_layout->setAlignment(Qt::AlignRight);

    auto* background_layout = new QHBoxLayout();
    background_layout->setAlignment(Qt::AlignRight);
    auto* background_color_lbl = new QLabel(tr("Background color: "));
    background_color_lbl->setAlignment(Qt::AlignRight);
    background_cp = new KColorPicker(true);
    connect(background_cp, &KColorPicker::colorChanged, [background_cp = background_cp, settings]() {
        auto preset = settings->getCurrentColorPreset();
        preset[EPresetsColorIdx::kBackgroundColor] = background_cp->color();
        settings->setCurrentColorPreset(preset);
        emit SettingsHandler::getInstance()->settingsChanged();
    });
    background_cp->setColor(current_preset[EPresetsColorIdx::kBackgroundColor]);
    background_layout->addWidget(background_color_lbl);
    background_layout->addWidget(background_cp);

    auto* canvas_layout = new QHBoxLayout();
    canvas_layout->setAlignment(Qt::AlignRight);
    auto* canvas_color_lbl = new QLabel(tr("Canvas color: "));
    canvas_cp = new KColorPicker(true);
    connect(canvas_cp, &KColorPicker::colorChanged, [canvas_cp = canvas_cp, settings]() {
        auto preset = settings->getCurrentColorPreset();
        preset[EPresetsColorIdx::kCanvasColor] = canvas_cp->color();
        settings->setCurrentColorPreset(preset);
        emit SettingsHandler::getInstance()->settingsChanged();
    });
    canvas_cp->setColor(current_preset[EPresetsColorIdx::kCanvasColor]);
    canvas_layout->addWidget(canvas_color_lbl);
    canvas_layout->addWidget(canvas_cp);

    auto* border_layout = new QHBoxLayout();
    border_layout->setAlignment(Qt::AlignRight);
    auto* border_color_lbl = new QLabel(tr("Border color: "));
    border_cp = new KColorPicker(true);
    connect(border_cp, &KColorPicker::colorChanged, [border_cp = border_cp, settings]() {
        auto preset = settings->getCurrentColorPreset();
        preset[EPresetsColorIdx::kBorderColor] = border_cp->color();
        settings->setCurrentColorPreset(preset);
        emit SettingsHandler::getInstance()->settingsChanged();
    });
    border_cp->setColor(current_preset[EPresetsColorIdx::kBorderColor]);
    border_layout->addWidget(border_color_lbl);
    border_layout->addWidget(border_cp);

    auto* text_layout = new QHBoxLayout();
    text_layout->setAlignment(Qt::AlignRight);
    auto* text_color_lbl = new QLabel(tr("Text color: "));
    text_cp = new KColorPicker(true);
    connect(text_cp, &KColorPicker::colorChanged, [text_cp = text_cp, settings]() {
        auto preset = settings->getCurrentColorPreset();
        preset[EPresetsColorIdx::kTextColor] = text_cp->color();
        settings->setCurrentColorPreset(preset);
        emit SettingsHandler::getInstance()->settingsChanged();
    });
    text_cp->setColor(current_preset[EPresetsColorIdx::kTextColor]);
    text_layout->addWidget(text_color_lbl);
    text_layout->addWidget(text_cp);

    auto* selection_layout = new QHBoxLayout();
    selection_layout->setAlignment(Qt::AlignRight);
    auto* selection_color_lbl = new QLabel(tr("Selection color: "));
    selection_cp = new KColorPicker(true);
    connect(selection_cp, &KColorPicker::colorChanged, [selection_cp = selection_cp, settings]() {
        auto preset = settings->getCurrentColorPreset();
        preset[EPresetsColorIdx::kSelectionColor] = selection_cp->color();
        settings->setCurrentColorPreset(preset);
        emit SettingsHandler::getInstance()->settingsChanged();
    });
    selection_cp->setColor(current_preset[EPresetsColorIdx::kSelectionColor]);
    selection_layout->addWidget(selection_color_lbl);
    selection_layout->addWidget(selection_cp);

    auto* menu_layout = new QHBoxLayout();
    menu_layout->setAlignment(Qt::AlignRight);
    auto* menu_color_lbl = new QLabel(tr("menu color: "));
    menu_cp = new KColorPicker(true);
    connect(menu_cp, &KColorPicker::colorChanged, [menu_cp = menu_cp, settings]() {
        auto preset = settings->getCurrentColorPreset();
        preset[EPresetsColorIdx::kMenuColor] = menu_cp->color();
        settings->setCurrentColorPreset(preset);
        emit SettingsHandler::getInstance()->settingsChanged();
    });
    menu_cp->setColor(current_preset[EPresetsColorIdx::kMenuColor]);
    menu_layout->addWidget(menu_color_lbl);
    menu_layout->addWidget(menu_cp);

    colors_layout->addLayout(background_layout);
    colors_layout->addLayout(canvas_layout);
    colors_layout->addLayout(border_layout);
    colors_layout->addLayout(text_layout);
    colors_layout->addLayout(selection_layout);
    colors_layout->addLayout(menu_layout);
    body_layout_->addLayout(colors_layout);
}


void ColorsWidget::sliderInit()
{
    auto* settings = SettingsHandler::getInstance();
    // slider init
    opacitySlider_->setFocusPolicy(Qt::NoFocus);
    opacitySlider_->setOrientation(Qt::Horizontal);
    opacitySlider_->setRange(0, 100);
    slider_layout_->setAlignment(Qt::AlignBottom);
    slider_layout_->addWidget(new QLabel(QStringLiteral("Master opacity:")));
    slider_layout_->addWidget(opacitySlider_);

    opacitySlider_->setMapedValue(0, settings->getCurrentOpacity(), 255);
    connect(opacitySlider_, &ExtendedSlider::valueChanged, [this]() {
        qDebug() << "Master opacity from settings = "
                 << SettingsHandler::getInstance()->masterOpacity();
        SettingsHandler::getInstance()->setCurrentOpacity(opacitySlider_->mappedValue(0, 255));
        //qDebug()<<"Opacity: "<<opacitySlider_->mappedValue(0, 255);
        emit SettingsHandler::getInstance()->settingsChanged();
    });
}


void ColorsWidget::saveResetBtnsInit()
{
    //presets_layout->setAlignment(Qt::AlignLeft);
    QPushButton* save_to_preset_btn = new QPushButton("Save to preset");
    connect(save_to_preset_btn, &QPushButton::clicked, this, &ColorsWidget::showPresetSaveWindow);

    QPushButton* reset_to_default_btn = new QPushButton("Reset to default");
    connect(reset_to_default_btn, &QPushButton::clicked, this, &ColorsWidget::resetCurrentPreset);

    bottom_layout_->addWidget(save_to_preset_btn);
    bottom_layout_->addWidget(reset_to_default_btn);
}

void ColorsWidget::showPresetSaveWindow()
{
    PresetSaveWindow* widget = new PresetSaveWindow(parentWidget());
    widget->show();
    centered_widget(this, widget);
}


void ColorsWidget::updateComponents()
{
    auto* settings = SettingsHandler::getInstance();
    auto current_preset = settings->getCurrentColorPreset();

    background_cp->setColor(current_preset[EPresetsColorIdx::kBackgroundColor]);
    canvas_cp->setColor(current_preset[EPresetsColorIdx::kCanvasColor]);
    border_cp->setColor(current_preset[EPresetsColorIdx::kBorderColor]);
    text_cp->setColor(current_preset[EPresetsColorIdx::kTextColor]);
    selection_cp->setColor(current_preset[EPresetsColorIdx::kSelectionColor]);
    menu_cp->setColor(current_preset[EPresetsColorIdx::kMenuColor]);
    opacitySlider_->setMapedValue(0, settings->getCurrentOpacity(), 255);

    emit SettingsHandler::getInstance()->settingsChanged();
}
