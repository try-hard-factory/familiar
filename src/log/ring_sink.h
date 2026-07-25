#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <QMutex>
#include <QObject>
#include <QStringList>

#include "quill/sinks/Sink.h"

namespace familiar::log {

// In-memory ring buffer of the most recent formatted log lines. Feeds
// DebugLogDialog's live tail and gives a crash handler something to dump
// even when the file sink hasn't flushed yet.
class RingSink final : public QObject, public quill::Sink
{
    Q_OBJECT

public:
    explicit RingSink(size_t capacity, QObject* parent = nullptr);

    void write_log(quill::MacroMetadata const* logMetadata,
                   uint64_t logTimestamp,
                   std::string_view threadId,
                   std::string_view threadName,
                   std::string const& processId,
                   std::string_view loggerName,
                   quill::LogLevel logLevel,
                   std::string_view logLevelDescription,
                   std::string_view logLevelShortCode,
                   std::vector<std::pair<std::string, std::string>> const* namedArgs,
                   std::string_view logMessage,
                   std::string_view logStatement) override;

    void flush_sink() noexcept override;
    void run_periodic_tasks() noexcept override {}

    QStringList entries() const;

signals:
    void entryAdded(const QString& line);

private:
    mutable QMutex mutex_;
    size_t capacity_;
    QStringList ring_;
};

} // namespace familiar::log
