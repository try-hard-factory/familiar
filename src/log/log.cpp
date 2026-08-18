#include "log.h"

#include <array>
#include <cstdio>
#include <memory>
#include <vector>

#ifdef _WIN32
// NOMINMAX: without it, <Windows.h> #defines min/max as raw text-
// substitution macros - textually alive for the REST of this
// translation unit, silently mangling every later `max(...)`/`.max()`
// call, including ones deep inside quill's own headers (quill/Backend.h
// below pulls in quill/backend/BackendWorker.h, which has several -
// confirmed via a real CI failure: dozens of cascading C2059/C2143/C4003
// syntax errors inside BackendWorker.h that made no sense until traced
// back to this). WIN32_LEAN_AND_MEAN trims <Windows.h> itself down
// (skips winsock/gdi/etc. headers this file has no use for) - not
// strictly required for the min/max fix, but standard practice
// alongside NOMINMAX and harmless here.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#include <Windows.h>
#include <DbgHelp.h>
#else
#include <unistd.h>
#endif

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScreen>
#include <QStandardPaths>
#include <QSysInfo>

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/sinks/ConsoleSink.h"
#include "quill/sinks/FileSink.h"

#include "utils/utils.h"

namespace familiar::log {
namespace {

// Brackets are baked into the registered logger name (rather than the
// pattern's literal text) so %(logger:<N) below pads the whole "[name]"
// token, pushing the alignment spaces outside the closing bracket instead
// of between the name and it.
constexpr std::array<const char*, 10> kChannelNames = {
    "[core]",
    "[scene]",
    "[view]",
    "[items]",
    "[undo]",
    "[io]",
    "[net]",
    "[settings]",
    "[ui]",
    "[qt]",
};

std::array<quill::Logger*, kChannelNames.size()> g_loggers{};
RingSink* g_ringSink = nullptr;
QString g_filePath;

#ifdef _WIN32
// quill's own SetUnhandledExceptionFilter (installed by
// quill::Backend::start() below, see include/quill/backend/
// SignalHandler.h's init_exception_handler()/on_exception()) already
// logs the exception code/name to familiar.log on a crash - real and
// useful (confirmed: "EXCEPTION_ACCESS_VIOLATION Code 3221225477" from a
// real crash), but gives no idea WHERE it happened. installCrashDumpHandler()
// below layers a real minidump on top - same EXCEPTION_POINTERS Windows
// already handed us, dumped via the plain WinAPI (Dbghelp.lib, no vcpkg
// dependency) instead of pulling in Breakpad/Crashpad for this. Chains
// to whatever filter was installed before it (quill's) so both still
// run - SetUnhandledExceptionFilter only keeps ONE filter active at a
// time, calling it again REPLACES the previous one rather than adding a
// second independent hook.
LPTOP_LEVEL_EXCEPTION_FILTER g_previousExceptionFilter = nullptr;

// Symbol-resolved call stack at the point of the crash - quill's own
// "backtrace" feature (LoggerImpl::init_backtrace(), see
// docs/backtrace_logging.rst) is a different thing entirely: a ring
// buffer of recent LOG MESSAGES flushed on error, not a C++ call stack.
// This walks the frames from the exception's own CONTEXT record via
// StackWalk64 - CaptureStackBackTrace() alone would only capture the
// CURRENT stack (this filter function's own frames, several levels
// above the actual fault inside Windows' own exception dispatch
// machinery), not where the crash happened.
QString captureStackTrace(CONTEXT* context)
{
    const HANDLE process = GetCurrentProcess();
    const HANDLE thread = GetCurrentThread();

    STACKFRAME64 frame = {};
    DWORD machineType = 0;
#if defined(_M_X64)
    machineType = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = context->Rip;
    frame.AddrFrame.Offset = context->Rbp;
    frame.AddrStack.Offset = context->Rsp;
#elif defined(_M_IX86)
    machineType = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = context->Eip;
    frame.AddrFrame.Offset = context->Ebp;
    frame.AddrStack.Offset = context->Esp;
#elif defined(_M_ARM64)
    machineType = IMAGE_FILE_MACHINE_ARM64;
    frame.AddrPC.Offset = context->Pc;
    frame.AddrFrame.Offset = context->Fp;
    frame.AddrStack.Offset = context->Sp;
#endif
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;

    QStringList lines;
    // Sanity cap - a corrupted stack (the whole reason we're here) can
    // otherwise walk garbage frames indefinitely.
    constexpr int kMaxFrames = 64;
    for (int i = 0; machineType != 0 && i < kMaxFrames; ++i) {
        if (!StackWalk64(machineType,
                         process,
                         thread,
                         &frame,
                         context,
                         nullptr,
                         SymFunctionTableAccess64,
                         SymGetModuleBase64,
                         nullptr)
            || frame.AddrPC.Offset == 0) {
            break;
        }

        QString line = QStringLiteral("#%1  0x%2")
                          .arg(i)
                          .arg(frame.AddrPC.Offset, 0, 16);

        alignas(SYMBOL_INFO) char symbolBuffer[sizeof(SYMBOL_INFO)
                                               + MAX_SYM_NAME];
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 symDisplacement = 0;
        if (SymFromAddr(process, frame.AddrPC.Offset, &symDisplacement, symbol)) {
            line += QStringLiteral(" %1+0x%2")
                       .arg(QString::fromLocal8Bit(symbol->Name,
                                                    int(symbol->NameLen)))
                       .arg(symDisplacement, 0, 16);

            IMAGEHLP_LINE64 srcLine = {};
            srcLine.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            DWORD lineDisplacement = 0;
            if (SymGetLineFromAddr64(process,
                                     frame.AddrPC.Offset,
                                     &lineDisplacement,
                                     &srcLine)) {
                line += QStringLiteral(" (%1:%2)")
                           .arg(QString::fromLocal8Bit(srcLine.FileName))
                           .arg(srcLine.LineNumber);
            }
        } else {
            line += QStringLiteral(" <no symbol - PDB not found/loaded?>");
        }
        lines << line;
    }
    return lines.isEmpty() ? QStringLiteral("<stack walk produced no frames>")
                           : lines.join(QStringLiteral("\n"));
}

LONG WINAPI writeMiniDumpAndChain(EXCEPTION_POINTERS* exceptionPointers)
{
    if (quill::Logger* core = channelLogger(Ch::Core)) {
        const QString trace = captureStackTrace(
            exceptionPointers->ContextRecord);
        FLOG_CRITICAL(Ch::Core, "Crash call stack:\n{}", trace.toStdString());
        core->flush_log(0);
    }

    const QString dumpDir = QFileInfo(g_filePath).absolutePath();
    const QString dumpPath
        = dumpDir + QStringLiteral("/crash_")
          + QDateTime::currentDateTime().toString(
              QStringLiteral("yyyyMMdd_HHmmss"))
          + QStringLiteral(".dmp");

    const HANDLE file = CreateFileW(
        reinterpret_cast<LPCWSTR>(dumpPath.utf16()),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mdei;
        mdei.ThreadId = GetCurrentThreadId();
        mdei.ExceptionPointers = exceptionPointers;
        mdei.ClientPointers = FALSE;
        // WithDataSegs, not a full/Complete dump - enough for a real
        // call stack with the RelWithDebInfo PDBs, without shipping
        // every byte of process memory back for a bug report.
        MiniDumpWriteDump(GetCurrentProcess(),
                          GetCurrentProcessId(),
                          file,
                          MiniDumpWithDataSegs,
                          &mdei,
                          nullptr,
                          nullptr);
        CloseHandle(file);
    }

    if (g_previousExceptionFilter) {
        return g_previousExceptionFilter(exceptionPointers);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

// Must run AFTER quill::Backend::start() (init() below) - that call is
// what installs quill's own filter in the first place, which this needs
// to capture as g_previousExceptionFilter to chain to.
void installCrashDumpHandler()
{
    // Loads once, up front, while things are calm - not from inside the
    // exception filter itself, where doing this for the first time would
    // add unnecessary risk/latency to an already-crashing process.
    // SYMOPT_DEFERRED_LOADS keeps this call itself fast: each module's
    // actual symbol table only loads on its first SymFromAddr()/
    // SymGetLineFromAddr64() call, i.e. lazily, right when
    // captureStackTrace() needs it.
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);

    g_previousExceptionFilter = SetUnhandledExceptionFilter(
        writeMiniDumpAndChain);
}
#endif

// Derives "<dir>/<base>_old.<ext>" from "<dir>/<base>.<ext>" (e.g.
// "familiar.log" -> "familiar_old.log"). Falls back to a plain "_old"
// suffix for the (currently unused) case of an extension-less --settings
// override path.
QString oldLogFilePath(const QString& filePath)
{
    const QFileInfo fi(filePath);
    const QString ext = fi.suffix();
    return ext.isEmpty() ? filePath + QStringLiteral("_old")
                         : fi.dir().filePath(fi.completeBaseName()
                                             + QStringLiteral("_old.") + ext);
}

bool stdoutIsTty()
{
#ifdef _WIN32
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}

Level moreVerbose(Level a, Level b)
{
    return static_cast<int>(a) < static_cast<int>(b) ? a : b;
}

void logSessionHeader(const QString& filePath)
{
    quill::Logger* core = channelLogger(Ch::Core);
    const std::string version = qApp->applicationVersion().toStdString();
    const std::string versionSuffix = version.empty() ? std::string()
                                                      : (version + " ");
    LOG_INFO(core,
             "familiar {}starting | qt {} | {} | log file: {}",
             versionSuffix,
             qVersion(),
             QSysInfo::prettyProductName().toStdString(),
             filePath.toStdString());

    const QList<QScreen*> screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        QScreen* screen = screens[i];
        const QRect geo = screen->geometry();
        LOG_INFO(core,
                 "screen {}: {}x{}+{}+{} @ {:.1f} dpi",
                 i,
                 geo.width(),
                 geo.height(),
                 geo.x(),
                 geo.y(),
                 double(screen->logicalDotsPerInch()));
    }
}

} // namespace

Level levelFromName(const QString& name, Level fallback)
{
    const QString upper = name.trimmed().toUpper();
    if (upper == QStringLiteral("TRACE")) {
        return Level::Trace;
    }
    if (upper == QStringLiteral("DEBUG")) {
        return Level::Debug;
    }
    if (upper == QStringLiteral("INFO")) {
        return Level::Info;
    }
    if (upper == QStringLiteral("WARNING") || upper == QStringLiteral("WARN")) {
        return Level::Warning;
    }
    if (upper == QStringLiteral("ERROR")) {
        return Level::Error;
    }
    if (upper == QStringLiteral("CRITICAL")
        || upper == QStringLiteral("FATAL")) {
        return Level::Critical;
    }
    return fallback;
}

namespace detail {

quill::LogLevel toQuillLevel(Level level)
{
    switch (level) {
    case Level::Trace:
        return quill::LogLevel::TraceL1;
    case Level::Debug:
        return quill::LogLevel::Debug;
    case Level::Info:
        return quill::LogLevel::Info;
    case Level::Warning:
        return quill::LogLevel::Warning;
    case Level::Error:
        return quill::LogLevel::Error;
    case Level::Critical:
        return quill::LogLevel::Critical;
    }
    return quill::LogLevel::Debug;
}

ScopeTimer::ScopeTimer(Ch channel,
                       const char* label,
                       const char* file,
                       int line,
                       const char* function)
    : logger_(channelLogger(channel))
    , label_(label)
    , file_(file)
    , line_(line)
    , function_(function)
    , start_(std::chrono::steady_clock::now())
{}

ScopeTimer::~ScopeTimer()
{
    const auto elapsed = std::chrono::steady_clock::now() - start_;
    const double ms = std::chrono::duration<double, std::milli>(elapsed).count();
    QUILL_LOG_RUNTIME_METADATA(logger_,
                               quill::LogLevel::Debug,
                               file_,
                               line_,
                               function_,
                               "{} took {:.3f} ms",
                               label_,
                               ms);
}

} // namespace detail

void init(const Options& options)
{
    quill::BackendOptions backendOptions;
    // Single-letter codes to match the design's "[I]"/"[W]" line format.
    backendOptions.log_level_short_codes
        = {"T", "T", "T", "D", "I", "N", "W", "E", "C", "B", "_", "D"};

    quill::SignalHandlerOptions signalOptions;
    signalOptions.logger = kChannelNames[static_cast<size_t>(Ch::Core)];

    quill::Backend::start<quill::FrontendOptions>(backendOptions, signalOptions);
#ifdef _WIN32
    installCrashDumpHandler();
#endif

    std::vector<std::shared_ptr<quill::Sink>> sinks;

    if (options.console && stdoutIsTty()) {
        auto consoleSink
            = quill::Frontend::create_or_get_sink<quill::ConsoleSink>(
                "console");
        consoleSink->set_log_level_filter(
            detail::toQuillLevel(options.consoleLevel));
        sinks.push_back(std::move(consoleSink));
    }

    QString filePath = options.filePath;
    if (filePath.isEmpty()) {
        QString dir = portableDataDir();
        if (dir.isEmpty()) {
            dir = QStandardPaths::writableLocation(
                QStandardPaths::AppLocalDataLocation);
        }
        filePath = dir + "/" + qApp->applicationName() + ".log";
    }
    g_filePath = filePath;

    // Keep only the last 2 sessions' worth of logs: promote whatever the
    // previous session left behind to "_old" (dropping anything older),
    // then start this session with a fresh file - no size/count-based
    // rotation within a session, just a straight swap on launch.
    const QString oldFilePath = oldLogFilePath(filePath);
    QFile::remove(oldFilePath);
    QFile::rename(filePath, oldFilePath);

    auto fileSink = quill::Frontend::create_or_get_sink<quill::FileSink>(
        filePath.toStdString(), []() {
            quill::FileSinkConfig cfg;
            cfg.set_open_mode('w');
            return cfg;
        }());
    fileSink->set_log_level_filter(detail::toQuillLevel(options.fileLevel));
    sinks.push_back(fileSink);

    auto ring = std::make_shared<RingSink>(options.ringCapacity);
    g_ringSink = ring.get();
    sinks.push_back(std::move(ring));

    const quill::PatternFormatterOptions
        pattern{"%(time) [%(log_level_short_code)] %(logger:<10) "
                "%(short_source_location) "
                "%(caller_function)() | %(message)",
                "%Y-%m-%d %H:%M:%S.%Qus",
                quill::Timezone::LocalTime};

    // Logger-level gate: the more verbose of console/file, so neither sink
    // starves - each sink still filters independently via its own level.
    const quill::LogLevel loggerLevel = detail::toQuillLevel(
        moreVerbose(options.consoleLevel, options.fileLevel));

    for (size_t i = 0; i < kChannelNames.size(); ++i) {
        g_loggers[i] = quill::Frontend::create_or_get_logger(kChannelNames[i],
                                                             sinks,
                                                             pattern);
        g_loggers[i]->set_log_level(loggerLevel);
    }

    logSessionHeader(filePath);
    detail::installQtMessageBridge();
}

void shutdown()
{
    if (quill::Logger* core = channelLogger(Ch::Core)) {
        core->flush_log();
    }
    quill::Backend::stop();
#ifdef _WIN32
    // Symmetric with installCrashDumpHandler()'s SymInitialize() - not
    // load-bearing for a normal exit (the process is going away anyway),
    // just tidy.
    SymCleanup(GetCurrentProcess());
#endif
}

void setChannelLevel(Ch channel, Level level)
{
    if (quill::Logger* logger = channelLogger(channel)) {
        logger->set_log_level(detail::toQuillLevel(level));
    }
}

quill::Logger* channelLogger(Ch channel)
{
    return g_loggers[static_cast<size_t>(channel)];
}

RingSink* ringSink()
{
    return g_ringSink;
}

QString logFilePath()
{
    return g_filePath;
}

} // namespace familiar::log
