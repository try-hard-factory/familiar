#include "log.h"

#include <array>
#include <cstdio>
#include <memory>
#include <vector>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <QApplication>
#include <QScreen>
#include <QStandardPaths>
#include <QSysInfo>

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/sinks/ConsoleSink.h"
#include "quill/sinks/RotatingFileSink.h"

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
    const std::string versionSuffix = version.empty() ? std::string() : (version + " ");
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

ScopeTimer::ScopeTimer(Ch channel, const char* label, const char* file, int line, const char* function)
    : logger_(channelLogger(channel))
    , label_(label)
    , file_(file)
    , line_(line)
    , function_(function)
    , start_(std::chrono::steady_clock::now())
{
}

ScopeTimer::~ScopeTimer()
{
    const auto elapsed = std::chrono::steady_clock::now() - start_;
    const double ms = std::chrono::duration<double, std::milli>(elapsed).count();
    QUILL_LOG_RUNTIME_METADATA(
        logger_, quill::LogLevel::Debug, file_, line_, function_, "{} took {:.3f} ms", label_, ms);
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

    std::vector<std::shared_ptr<quill::Sink>> sinks;

    if (options.console && stdoutIsTty()) {
        auto consoleSink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console");
        consoleSink->set_log_level_filter(detail::toQuillLevel(options.consoleLevel));
        sinks.push_back(std::move(consoleSink));
    }

    QString filePath = options.filePath;
    if (filePath.isEmpty()) {
        filePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                   + "/" + qApp->applicationName() + ".log";
    }

    auto fileSink = quill::Frontend::create_or_get_sink<quill::RotatingFileSink>(
        filePath.toStdString(),
        [&options]()
        {
            quill::RotatingFileSinkConfig cfg;
            cfg.set_open_mode('a');
            cfg.set_rotation_max_file_size(options.rotateBytes);
            cfg.set_max_backup_files(uint32_t(options.rotateCount));
            return cfg;
        }());
    fileSink->set_log_level_filter(detail::toQuillLevel(options.fileLevel));
    sinks.push_back(fileSink);

    auto ring = std::make_shared<RingSink>(options.ringCapacity);
    g_ringSink = ring.get();
    sinks.push_back(std::move(ring));

    const quill::PatternFormatterOptions pattern{
        "%(time) [%(log_level_short_code)] %(logger:<10) %(short_source_location) "
        "%(caller_function)() | %(message)",
        "%H:%M:%S.%Qus",
        quill::Timezone::LocalTime};

    // Logger-level gate: the more verbose of console/file, so neither sink
    // starves - each sink still filters independently via its own level.
    const quill::LogLevel loggerLevel
        = detail::toQuillLevel(moreVerbose(options.consoleLevel, options.fileLevel));

    for (size_t i = 0; i < kChannelNames.size(); ++i) {
        g_loggers[i] = quill::Frontend::create_or_get_logger(kChannelNames[i], sinks, pattern);
        g_loggers[i]->set_log_level(loggerLevel);
    }

    logSessionHeader(filePath);
    detail::installQtMessageBridge();
}

void shutdown()
{
    if (quill::Logger* core = channelLogger(Ch::Core))
        core->flush_log();
    quill::Backend::stop();
}

void setChannelLevel(Ch channel, Level level)
{
    if (quill::Logger* logger = channelLogger(channel))
        logger->set_log_level(detail::toQuillLevel(level));
}

quill::Logger* channelLogger(Ch channel)
{
    return g_loggers[static_cast<size_t>(channel)];
}

RingSink* ringSink()
{
    return g_ringSink;
}

} // namespace familiar::log
