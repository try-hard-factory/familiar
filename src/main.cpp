#include "mainwindow.h"

#include <Logger.h>
#include <QApplication>
#include <QMetaType>

/*!
 * \~russian \mainpage RU
 *
 * \~russian \section intro_sec Introduction
 *
 * \~russian This is the introduction.
 *
 * \~russian \section install_sec Installation
 *
 * \~russian \subsection step1 Step 1: Opening the box
 *
 * \~russian etc...
 *
 * \~english \mainpage ENG
 *
 * \~english \section intro_sec Introduction
 *
 * \~english This is the introduction.
 *
 * \~english \section install_sec Installation
 *
 * \~english \subsection step1 Step 1: Opening the box
 *
 * \~english etc...
 */


Logger logger;

int main(int argc, char* argv[])
{
#ifdef NDEBUG
    qDebug() << "NDEBUG DEFINED";
#else
    qDebug() << "NDEBUG notDEFINED";
#endif

    qRegisterMetaType<QMap<int, QColor>>("QMap<int, QColor>");
    qRegisterMetaType<QMap<int, int>>("QMap<int, int>");

    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}