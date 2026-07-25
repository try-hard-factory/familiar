#include "ring_sink.h"

#include <QMutexLocker>

namespace familiar::log {

RingSink::RingSink(size_t capacity, QObject* parent)
    : QObject(parent)
    , capacity_(capacity)
{
}

void RingSink::write_log(quill::MacroMetadata const* /*logMetadata*/,
                          uint64_t /*logTimestamp*/,
                          std::string_view /*threadId*/,
                          std::string_view /*threadName*/,
                          std::string const& /*processId*/,
                          std::string_view /*loggerName*/,
                          quill::LogLevel /*logLevel*/,
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

    {
        QMutexLocker locker(&mutex_);
        ring_.append(line);
        while (size_t(ring_.size()) > capacity_)
            ring_.removeFirst();
    }

    emit entryAdded(line);
}

void RingSink::flush_sink() noexcept
{
}

QStringList RingSink::entries() const
{
    QMutexLocker locker(&mutex_);
    return ring_;
}

} // namespace familiar::log
