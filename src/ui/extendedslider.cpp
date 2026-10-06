// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "extendedslider.h"

ExtendedSlider::ExtendedSlider(QWidget* parent)
    : QSlider(parent)
{
    connect(this,
            &ExtendedSlider::valueChanged,
            this,
            &ExtendedSlider::update_tooltip);
    connect(this,
            &ExtendedSlider::sliderMoved,
            this,
            &ExtendedSlider::fire_timer);
    mTimer_.setSingleShot(true);
    connect(&mTimer_,
            &QTimer::timeout,
            this,
            &ExtendedSlider::modifications_ended);
}

int ExtendedSlider::mapped_value(int min, int max)
{
    const qreal progress = ((value() - minimum()))
                     / static_cast<qreal>(maximum() - minimum());
    return min + static_cast<int>((max - min) * progress);
}

void ExtendedSlider::set_maped_value(int min, int val, int max)
{
    const qreal progress = ((val - min) + 1) / static_cast<qreal>(max - min);
    const int value
        = minimum() + static_cast<int>((maximum() - minimum()) * progress);
    setValue(value);
}

void ExtendedSlider::update_tooltip()
{
    setToolTip(QString::number(value()) + "%");
}

void ExtendedSlider::fire_timer()
{
    mTimer_.start(500);
}
