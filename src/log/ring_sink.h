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

// Full definition: log.h - forward-declared here only, since log.h itself
// includes this header (for RingSink) before it defines Level. A
// forward-declared scoped enum is a complete-enough type (fixed, implicit
// int underlying type) to use by value/as a struct member below - just
// not by enumerator name - so this doesn't need the full definition.
enum class Level;

// In-memory ring buffer of the most recent formatted log lines, each
// paired with its actual level (mapped down from quill::LogLevel - see
// ring_sink.cpp's fromQuillLevel()) so a consumer can filter by level
// without re-parsing the formatted text. Feeds DebugLogDialog's live tail
// (including its per-level filter checkboxes) and gives a crash handler
// something to dump even when the file sink hasn't flushed yet.
class RingSink final : public QObject, public quill::Sink
{
    Q_OBJECT

public:
    explicit RingSink(size_t capacity, QObject* parent = nullptr);

    void write_log(
        quill::MacroMetadata const* logMetadata,
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

    struct Entry
    {
        Level level;
        QString line;
    };

    QList<Entry> entries() const;

signals:
    void entryAdded(Level level, const QString& line);

private:
    mutable QMutex mutex_;
    size_t capacity_;
    QList<Entry> ring_;
};

} // namespace familiar::log
