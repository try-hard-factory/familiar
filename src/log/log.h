#pragma once

// Public facade for familiar's logger. This is the ONLY header app code
// should include for logging - no other file may include quill/fmt headers
// directly. See memory/familiar_logger_design.md for the full design.

#include <chrono>
#include <cstddef>

#include <QDebug>
#include <QString>

#include "quill/LogMacros.h"
#include "quill/Logger.h"

#include "qt_formatters.h"
#include "ring_sink.h"

namespace familiar::log {

enum class Level { Trace, Debug, Info, Warning, Error, Critical };

enum class Ch { Core, Scene, View, Items, Undo, IO, Net, Settings, UI, Qt };

struct Options
{
    Level consoleLevel = Level::Debug;
    Level fileLevel = Level::Debug;
    QString filePath; // default: AppLocalDataLocation/<appName>.log
    bool console = true; // auto-off when stdout isn't a tty
    size_t ringCapacity = 2000;
};

// Parses a case-insensitive level name (TRACE/DEBUG/INFO/WARNING or WARN/
// ERROR/CRITICAL or FATAL) - for the --loglevel command-line option;
// unrecognized input falls back to `fallback`.
Level levelFromName(const QString& name, Level fallback = Level::Info);

// First line of main(), before MainWindow is constructed.
void init(const Options& options = Options{});

// Final flush, called at the end of main().
void shutdown();

// Runtime per-channel level tuning.
void setChannelLevel(Ch channel, Level level);

// Whether the Qt bridge (qDebug()/qWarning()/...) logs the full C++
// function signature Qt's Q_FUNC_INFO captures (return type + every
// parameter type) or just "Class::method". Off (short names) by default -
// full signatures get unreadable fast around templates/lambdas/mixins.
void setQtBridgeVerboseFunctions(bool verbose);

quill::Logger* channelLogger(Ch channel);

// Captures whatever operator<<(QDebug, const T&) already prints for a type
// that has Qt debug-stream support but no fmtquill::formatter of its own
// (QGraphicsItem*, QList<T>, enums with QDebug support, ...). Use as
// FLOG_DEBUG(Ch::X, "item: {}", familiar::log::debugString(item)).
template<typename T>
QString debugString(const T& value)
{
    QString result;
    QDebug(&result) << value;
    return result;
}

// Last N formatted lines, for DebugLogDialog's live tail; returns nullptr
// if init() hasn't run yet.
RingSink* ringSink();

// Resolved path of the current session's log file (after any default
// AppLocalDataLocation fallback in init()) - the single source of truth
// for anything that needs to show/open it (DebugLogDialog, ...). Empty
// if init() hasn't run yet.
QString logFilePath();

namespace detail {
quill::LogLevel toQuillLevel(Level level);

// Installs the qInstallMessageHandler bridge that routes qDebug()/qWarning()/
// etc. into Ch::Qt. Called once by init().
void installQtMessageBridge();

// Small RAII helper backing FLOG_TIMER: logs the elapsed time at scope
// exit, attributed to the FLOG_TIMER call site rather than log.h itself.
class ScopeTimer
{
public:
    ScopeTimer(Ch channel,
               const char* label,
               const char* file,
               int line,
               const char* function);
    ~ScopeTimer();

    ScopeTimer(const ScopeTimer&) = delete;
    ScopeTimer& operator=(const ScopeTimer&) = delete;

private:
    quill::Logger* logger_;
    const char* label_;
    const char* file_;
    int line_;
    const char* function_;
    std::chrono::steady_clock::time_point start_;
};
} // namespace detail

} // namespace familiar::log

#define FML_LOG_CONCAT_INNER(a, b) a##b
#define FML_LOG_CONCAT(a, b) FML_LOG_CONCAT_INNER(a, b)

#define FLOG_TRACE(channel, ...) \
    LOG_TRACE_L1(::familiar::log::channelLogger(channel), __VA_ARGS__)
#define FLOG_DEBUG(channel, ...) \
    LOG_DEBUG(::familiar::log::channelLogger(channel), __VA_ARGS__)
#define FLOG_INFO(channel, ...) \
    LOG_INFO(::familiar::log::channelLogger(channel), __VA_ARGS__)
#define FLOG_WARN(channel, ...) \
    LOG_WARNING(::familiar::log::channelLogger(channel), __VA_ARGS__)
#define FLOG_ERROR(channel, ...) \
    LOG_ERROR(::familiar::log::channelLogger(channel), __VA_ARGS__)
#define FLOG_CRITICAL(channel, ...) \
    LOG_CRITICAL(::familiar::log::channelLogger(channel), __VA_ARGS__)

// RAII scope timer: logs the elapsed wall time when the enclosing scope exits.
#define FLOG_TIMER(channel, label) \
    ::familiar::log::detail::ScopeTimer FML_LOG_CONCAT(flog_timer_, __LINE__) \
    { \
        channel, label, __FILE__, __LINE__, __FUNCTION__ \
    }

// Debug-level log, rate-limited to once every n occurrences - for hot paths
// like mouseMoveEvent/paintEvent.
#define FLOG_EVERY_N(channel, n, ...) \
    QUILL_LOG_DEBUG_LIMIT_EVERY_N(n, \
                                  ::familiar::log::channelLogger(channel), \
                                  __VA_ARGS__)

// Debug-level log that fires only the first time this call site is reached.
#define FLOG_ONCE(channel, ...) \
    do { \
        static bool flog_once_logged_ = false; \
        if (!flog_once_logged_) { \
            flog_once_logged_ = true; \
            FLOG_DEBUG(channel, __VA_ARGS__); \
        } \
    } while (false)
