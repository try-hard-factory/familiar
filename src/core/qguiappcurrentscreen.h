/**
* \file qguiappcurrentscreen.h
* \author max
* Created on Sun May  1 14:54:59 2022
*/

#ifndef QGUIAPPCURRENTSCREEN_H
#define QGUIAPPCURRENTSCREEN_H

#include <QPoint>

class QScreen;

class QGuiAppCurrentScreen
{
public:
    explicit QGuiAppCurrentScreen();
    QScreen* currentScreen();
    QScreen* currentScreen(const QPoint& pos);

private:
    QScreen* screenAt(const QPoint& pos);

    // class members
private:
    QScreen* m_currentScreen;
};

#endif
