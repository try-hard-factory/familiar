#include "colors_widget.h"
#include <QVBoxLayout>

ColorsWidget::ColorsWidget(QWidget *parent)
    : QWidget{parent}
{
    layout_ = new QVBoxLayout(this);
    layout_->setAlignment(Qt::AlignTop);
}

void ColorsWidget::updateComponents()
{

}
