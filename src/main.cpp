#include "mainwindow.h"

#include <QApplication>
#include <QIcon>
#include <QMetaType>

#include "core/settings.h"
#include "log/log.h"
using namespace familiar::log;

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

int main(int argc, char* argv[])
{
#ifdef Q_OS_LINUX
    // The Wayland QPA plugin has been unreliable/slow on this app; xcb
    // (X11, via XWayland where needed) is the known-good path. Only
    // applies if the user hasn't already chosen a platform themselves.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "xcb");
#endif

    qRegisterMetaType<QMap<int, QColor>>("QMap<int, QColor>");
    qRegisterMetaType<QMap<int, int>>("QMap<int, int>");

    QApplication a(argc, argv);
    // Needed for a sane default settings file location
    // (QStandardPaths::AppConfigLocation, see
    // SettingsHandler::SettingsHandler() in core/settingshandler.cpp) -
    // without it, applicationName() is empty and that path degenerates.
    // Organization name deliberately left unset - Qt nests
    // AppConfigLocation under BOTH organizationName and applicationName
    // when both are set, which produced ".config/familiar/familiar/".
    a.setApplicationName(QStringLiteral("familiar"));

    // Must run before anything below reads a CommandlineArgs getter (log
    // level, filename to open, ...) - see CommandlineArgs::process().
    CommandlineArgs::instance().process(a);

    // Was defined but never actually called - the image allocation limit
    // never took effect at startup, only whenever something happened to
    // resave settings afterward.
    FamSettings().onStartup();

    // Window/taskbar icon. Built from the raster PNGs (rather than the
    // .svg also in graphics.qrc) since QIcon needs the Qt SVG icon-engine
    // plugin at runtime to rasterize an SVG source, and that plugin isn't
    // guaranteed to be deployed everywhere this app runs.
    QIcon appIcon;
    appIcon.addFile(QStringLiteral(":/img/app/familiar_256.png"));
    appIcon.addFile(QStringLiteral(":/img/app/familiar_512.png"));
    a.setWindowIcon(appIcon);

    Options logOptions;
    logOptions.consoleLevel = levelFromName(
        CommandlineArgs::instance().loglevel());
    familiar::log::init(logOptions);


#ifdef NDEBUG
    FLOG_DEBUG(Ch::Core, "NDEBUG defined");
#else
    FLOG_DEBUG(Ch::Core, "NDEBUG not defined");
#endif

    MainWindow w;
    // Not a bare show() - if there's anything left over from a session
    // that didn't exit cleanly, this shows the recovery prompt first and
    // defers actually showing the main window until it's dismissed (see
    // MainWindow::showOrOfferRecovery()).
    w.showOrOfferRecovery();

    const QString startupFile = CommandlineArgs::instance().filename();
    if (!startupFile.isEmpty()) {
        w.fileActions().processOpenFile(startupFile);
    }

    const int result = a.exec();

    familiar::log::shutdown();
    return result;
}
