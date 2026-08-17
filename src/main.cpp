#include "mainwindow.h"

#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include <QMetaType>
#include <QSharedMemory>

#include "core/settings.h"
#include "log/log.h"
using namespace familiar::log;

#include <gtest/gtest.h>

#include "support/settings_test_environment.h"

namespace {

// Single-instance guard. QSharedMemory is the
// standard cross-platform "is another copy of me already running"
// primitive - unlike a plain lock file, the OS reclaims it if the owning
// process is killed outright (SIGKILL, power loss), so a genuinely dead
// instance doesn't permanently wedge future launches.
//
// One known rough edge, Linux/Unix-specific: if the FIRST instance
// crashes (not killed, not a clean exit - e.g. segfault) without ever
// detaching, the underlying System V segment can occasionally survive
// with no live owner, which would make create() below keep failing even
// though nothing is actually running. attach()-then-detach() first is
// the standard mitigation (forces Qt to re-examine/release a segment
// nothing is really attached to any more) - not a 100% guarantee on
// every platform/Qt version, but good enough for "don't start a second
// window" without reaching for a heavier IPC-based liveness check.
bool acquireSingleInstanceLock(QSharedMemory& guard)
{
    if (guard.attach())
        guard.detach();
    return guard.create(1);
}

} // namespace


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

    // Checked here, before anything below sets up real app state
    // (single-instance lock, SettingsHandler, MainWindow) - "familiar
    // -t" runs the GoogleTest suite in place of the app proper and
    // exits, it doesn't launch a window alongside it.
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("-t")) {
            ::testing::InitGoogleTest(&argc, argv);
            // Owned by GoogleTest from here on (::testing::Environment's
            // documented contract - it deletes registered environments
            // itself during RUN_ALL_TESTS() teardown), not leaked.
            ::testing::AddGlobalTestEnvironment(new SettingsTestEnvironment());
            return RUN_ALL_TESTS();
        }
    }

    // Needed for a sane default settings file location
    // (QStandardPaths::AppConfigLocation, see
    // SettingsHandler::SettingsHandler() in core/settingshandler.cpp) -
    // without it, applicationName() is empty and that path degenerates.
    // Organization name deliberately left unset - Qt nests
    // AppConfigLocation under BOTH organizationName and applicationName
    // when both are set, which produced ".config/familiar/familiar/".
    a.setApplicationName(QStringLiteral("familiar"));

    // Declared here (not in a narrower scope) so it stays alive - and
    // the lock held - for the whole process lifetime, releasing only
    // when main() itself returns. Key is fixed, not derived from
    // anything user-configurable (e.g. --settings <path>) - two
    // instances pointed at two different settings files are still two
    // windows, which is exactly what this guards against.
    QSharedMemory singleInstanceGuard(
        QStringLiteral("familiar-single-instance-9f3b2c7a"));
    if (!acquireSingleInstanceLock(singleInstanceGuard)) {
        // Plain native QMessageBox, not this app's own custom-chrome one
        // (widgets/message_box.h) - this fires before SettingsHandler/
        // the color preset system are initialized at all, and exiting
        // right after is the whole point of this path; not worth
        // standing up the themed dialog machinery just for this one
        // early, rare message.
        QMessageBox::information(
            nullptr,
            QObject::tr("Familiar is already running"),
            QObject::tr(
                "Another instance of Familiar is already open. Only one "
                "window is supported right now."));
        return 0;
    }

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
    // defers actually showing the main window (and opening startupFile
    // below, if any) until it's dismissed - see
    // MainWindow::showOrOfferRecovery()'s own comment for why that
    // ordering matters.
    w.showOrOfferRecovery(CommandlineArgs::instance().filename());

    const int result = a.exec();

    familiar::log::shutdown();
    return result;
}
