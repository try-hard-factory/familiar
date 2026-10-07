#include "log.h"

#include <atomic>
#include <cstdlib>
#include <string>
#include <string_view>

#include <QMessageLogContext>
#include <QtGlobal>

namespace familiar::log {
namespace {
std::atomic<bool> gVerboseFunctions{false};
} // namespace

void set_qt_bridge_verbose_functions(bool verbose)
{
    gVerboseFunctions.store(verbose, std::memory_order_relaxed);
}

namespace detail {
namespace {

Level level_from_qt(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return Level::Debug;
    case QtInfoMsg:
        return Level::Info;
    case QtWarningMsg:
        return Level::Warning;
    case QtCriticalMsg:
        return Level::Error;
    case QtFatalMsg:
        return Level::Critical;
    default:
        return Level::Debug;
    }
}

// Qt's Q_FUNC_INFO (what QMessageLogContext::function holds) is the
// compiler's full decorated signature - return type, template args, every
// parameter type. Collapse it to "Class::method", the way the old
// Logger::stripFunctionName() did, unless verbose mode is on. The pattern
// formatter appends the trailing "()" itself, so this returns the name bare.
std::string short_function_name(std::string_view prettyName)
{
    const size_t parenPos = prettyName.find('(');
    if (parenPos == std::string_view::npos) {
        return std::string(prettyName);
    }

    const std::string_view beforeParen = prettyName.substr(0, parenPos);
    const size_t spacePos = beforeParen.rfind(' ');
    const size_t begin = (spacePos == std::string_view::npos) ? 0
                                                              : spacePos + 1;
    if (begin > parenPos) {
        return std::string(prettyName);
    }

    return std::string(prettyName.substr(begin, parenPos - begin));
}

void qt_message_handler(QtMsgType type,
                        const QMessageLogContext& context,
                        const QString& msg)
{
    quill::Logger* logger = channel_logger(Ch::Qt);
    const quill::LogLevel level = to_quill_level(level_from_qt(type));
    const char* file = context.file ? context.file : "unknown";

    const std::string function = !context.function ? std::string("unknown")
                                 : gVerboseFunctions.load(
                                       std::memory_order_relaxed)
                                     ? std::string(context.function)
                                     : short_function_name(context.function);
    const std::string text = msg.toStdString();

    QUILL_LOG_RUNTIME_METADATA(logger,
                               level,
                               file,
                               context.line,
                               function.c_str(),
                               "{}",
                               text);

    if (type == QtFatalMsg) {
        logger->flush_log();
        std::abort();
    }
}

} // namespace

void install_qt_message_bridge()
{
    qInstallMessageHandler(qt_message_handler);
}

} // namespace detail
} // namespace familiar::log
