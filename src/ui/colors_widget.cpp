#include "colors_widget.h"
#include <kColorPicker/KColorPicker.h>
#include <QVBoxLayout>

using kColorPicker::KColorPicker;

ColorsWidget::ColorsWidget(QWidget* parent)
    : QWidget(parent)
    , layout_(new QVBoxLayout(this))
{
    layout_->setAlignment(Qt::AlignTop);
    auto colorPicker = new KColorPicker(true);
    colorPicker->setColor(QColor(Qt::red));
    layout_->addWidget(colorPicker);
    setLayout(layout_);
}

void ColorsWidget::updateComponents() {}
