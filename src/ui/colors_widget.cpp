#include "colors_widget.h"
#include <kColorPicker/KColorPicker.h>
#include <ui/extendedslider.h>
#include <QButtonGroup>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <core/settingshandler.h>

using kColorPicker::KColorPicker;

ColorsWidget::ColorsWidget(QWidget* parent)
    : QWidget(parent)
    , layout_(new QVBoxLayout(this))
    , opacitySlider_(new ExtendedSlider())
{
    layout_->setAlignment(Qt::AlignTop);

    auto* header_layout = new QHBoxLayout();
    auto* presets_lbl = new QLabel(tr("Presets"));
    presets_lbl->setAlignment(Qt::AlignLeft);
    auto* colors_lbl = new QLabel(tr("Colors"));
    colors_lbl->setAlignment(Qt::AlignCenter);
    header_layout->addWidget(presets_lbl);
    header_layout->addWidget(colors_lbl);

    auto* body_layout = new QHBoxLayout();

    auto* presets_layout = new QVBoxLayout();
    presets_layout->setAlignment(Qt::AlignLeft);
    QPushButton* dark_btn = new QPushButton("Dark");
    QPushButton* light_btn = new QPushButton("Light");
    QPushButton* custom1_btn = new QPushButton("Custom 1");
    QPushButton* custom2_btn = new QPushButton("Custom 2");
    QPushButton* custom3_btn = new QPushButton("Custom 3");
    QPushButton* custom4_btn = new QPushButton("Custom 4");


    presets_layout->addWidget(presets_lbl);
    presets_layout->addWidget(dark_btn);
    presets_layout->addWidget(light_btn);
    presets_layout->addWidget(custom1_btn);
    presets_layout->addWidget(custom2_btn);
    presets_layout->addWidget(custom3_btn);
    presets_layout->addWidget(custom4_btn);
    body_layout->addLayout(presets_layout);


    auto* colors_layout = new QVBoxLayout();
    colors_layout->setAlignment(Qt::AlignRight);

    auto* background_layout = new QHBoxLayout();
    background_layout->setAlignment(Qt::AlignRight);
    auto* background_color_lbl = new QLabel(tr("Background color: "));
    background_color_lbl->setAlignment(Qt::AlignRight);
    auto* background_cp = new KColorPicker(true);
    background_cp->setColor(QColor(Qt::red));
    background_layout->addWidget(background_color_lbl);
    background_layout->addWidget(background_cp);

    auto* canvas_layout = new QHBoxLayout();
    canvas_layout->setAlignment(Qt::AlignRight);
    auto* canvas_color_lbl = new QLabel(tr("Canvas color: "));
    auto* canvas_cp = new KColorPicker(true);
    canvas_cp->setColor(QColor(Qt::red));
    canvas_layout->addWidget(canvas_color_lbl);
    canvas_layout->addWidget(canvas_cp);

    auto* border_layout = new QHBoxLayout();
    border_layout->setAlignment(Qt::AlignRight);
    auto* border_color_lbl = new QLabel(tr("Border color: "));
    auto* border_cp = new KColorPicker(true);
    border_cp->setColor(QColor(Qt::red));
    border_layout->addWidget(border_color_lbl);
    border_layout->addWidget(border_cp);

    auto* text_layout = new QHBoxLayout();
    text_layout->setAlignment(Qt::AlignRight);
    auto* text_color_lbl = new QLabel(tr("Text color: "));
    auto* text_cp = new KColorPicker(true);
    text_cp->setColor(QColor(Qt::red));
    text_layout->addWidget(text_color_lbl);
    text_layout->addWidget(text_cp);

    auto* selection_layout = new QHBoxLayout();
    selection_layout->setAlignment(Qt::AlignRight);
    auto* selection_color_lbl = new QLabel(tr("Selection color: "));
    auto* selection_cp = new KColorPicker(true);
    selection_cp->setColor(QColor(Qt::red));
    selection_layout->addWidget(selection_color_lbl);
    selection_layout->addWidget(selection_cp);

    // auto* selection_layout = new QHBoxLayout();
    // auto* color5 = new QLabel(tr("..reserved.."));

    colors_layout->addLayout(background_layout);
    colors_layout->addLayout(canvas_layout);
    colors_layout->addLayout(border_layout);
    colors_layout->addLayout(text_layout);
    colors_layout->addLayout(selection_layout);

    // colors_layout->addWidget(color5);
    body_layout->addLayout(colors_layout);


    opacitySlider_->setFocusPolicy(Qt::NoFocus);
    opacitySlider_->setOrientation(Qt::Horizontal);
    opacitySlider_->setRange(0, 100);
    auto* localLayout = new QHBoxLayout();
    localLayout->setAlignment(Qt::AlignBottom);
    localLayout->addWidget(new QLabel(QStringLiteral("Master opacity:")));
    localLayout->addWidget(opacitySlider_);

    // ExtendedSlider* opacitySlider = opacitySlider_;
    // connect(opacitySlider_,
    //         &ExtendedSlider::valueChanged,
    //         this,
    //         [labelMsg, label, opacitySlider](int val) {
    //             label->setText(labelMsg.arg(val));
    //             ConfigHandler().setContrastOpacity(
    //               opacitySlider->mappedValue(0, 255));
    //         });
    // m_layout->addWidget(label);


    // int opacity = ConfigHandler().contrastOpacity();
    opacitySlider_->setMapedValue(0, /*opacity*/ 255, 255);
    connect(opacitySlider_,
            &ExtendedSlider::valueChanged,
            [this]() {
                SettingsHandler::getInstance()->setMasterOpacity(opacitySlider_->mappedValue(0, 255));
                //qDebug()<<"Opacity: "<<opacitySlider_->mappedValue(0, 255);
                emit SettingsHandler::getInstance()->settingsChanged();
            });

    layout_->addLayout(header_layout);
    layout_->addLayout(body_layout);
    layout_->addSpacing(50);
    layout_->addLayout(localLayout);


    setLayout(layout_);
}


ColorsWidget::~ColorsWidget()
{
    delete layout_;
}

void ColorsWidget::updateComponents() {}
