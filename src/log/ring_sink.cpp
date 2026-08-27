#include "ring_sink.h"

#include <QMutexLocker>

#include "log.h" // for the real Level enum - only forward-declared in ring_sink.h

namespace familiar::log {

namespace {

// quill has 3 trace sub-levels (only TraceL1 is ever actually emitted by
// this app's own FLOG_TRACE - see log.h's toQuillLevel()) plus Notice/
// Backtrace/None/Dynamic, none of which familiar::log::Level has a slot
// for (this app never logs at those). Collapsed onto the closest real
// severity rather than dropped, so a stray one still shows up somewhere
// sensible instead of vanishing from the ring entirely.
Level fromQuillLevel(quill::LogLevel level)
{
    switch (level) {
    case quill::LogLevel::TraceL3:
    case quill::LogLevel::TraceL2:
    case quill::LogLevel::TraceL1:
        return Level::Trace;
    case quill::LogLevel::Debug:
        return Level::Debug;
    case quill::LogLevel::Info:
    case quill::LogLevel::Notice:
        return Level::Info;
    case quill::LogLevel::Warning:
        return Level::Warning;
    case quill::LogLevel::Error:
        return Level::Error;
    case quill::LogLevel::Critical:
    case quill::LogLevel::Backtrace:
        return Level::Critical;
    case quill::LogLevel::None:
    case quill::LogLevel::Dynamic:
        break;
    }
    return Level::Info;
}

} // namespace

RingSink::RingSink(size_t capacity, QObject* parent)
    : QObject(parent)
    , capacity_(capacity)
{}

void RingSink::write_log(
    quill::MacroMetadata const* /*logMetadata*/,
    uint64_t /*logTimestamp*/,
    std::string_view /*threadId*/,
    std::string_view /*threadName*/,
    std::string const& /*processId*/,
    std::string_view /*loggerName*/,
    quill::LogLevel logLevel,
    std::string_view /*logLevelDescription*/,
    std::string_view /*logLevelShortCode*/,
    std::vector<std::pair<std::string, std::string>> const* /*namedArgs*/,
    std::string_view /*logMessage*/,
    std::string_view logStatement)
{
    // logStatement is newline-terminated; strip it for QPlainTextEdit-style consumers.
    const size_t len = !logStatement.empty() && logStatement.back() == '\n'
                           ? logStatement.size() - 1
                           : logStatement.size();
    const QString line = QString::fromUtf8(logStatement.data(), int(len));
    const Level level = fromQuillLevel(logLevel);

    {
        QMutexLocker locker(&mutex_);
        ring_.append({level, line});
        while (size_t(ring_.size()) > capacity_) {
            ring_.removeFirst();
        }
    }

    emit entryAdded(level, line);
}

void RingSink::flush_sink() noexcept {}

QList<RingSink::Entry> RingSink::entries() const
{
    QMutexLocker locker(&mutex_);
    return ring_;
}

} // namespace familiar::log
