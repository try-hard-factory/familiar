#include "preferencesconf.h"
#include <QVBoxLayout>

PreferencesConf::PreferencesConf(QWidget* parent)
    : QWidget{parent}
{
    layout_ = new QVBoxLayout(this);
    layout_->setAlignment(Qt::AlignTop);
}

void PreferencesConf::updateComponents() {}
