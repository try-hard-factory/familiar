// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#pragma once

#include <QSlider>
#include <QTimer>

class ExtendedSlider : public QSlider
{
    Q_OBJECT
public:
    explicit ExtendedSlider(QWidget* parent = nullptr);

    int mapped_value(int min, int max);
    void set_maped_value(int min, int val, int max);

signals:
    void modifications_ended();

private slots:
    void update_tooltip();
    void fire_timer();

private:
    QTimer mTimer_;
};
