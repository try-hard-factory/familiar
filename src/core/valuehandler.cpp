#include "valuehandler.h"

#include <QVariant>

QVariant ValueHandler::value(const QVariant& val)
{
    if (!val.isValid() || !check(val)) {
        return fallback();
    } else {
        return process(val);
    }
}

QVariant ValueHandler::fallback()
{
    return {};
}

QVariant ValueHandler::representation(const QVariant& val)
{
    return val.toString();
}

QString ValueHandler::expected()
{
    return {};
}

QVariant ValueHandler::process(const QVariant& val)
{
    return val;
}

// KEY SEQUENCE

KeySequence::KeySequence(const QKeySequence& fallback)
  : fallback_(fallback)
{}

bool KeySequence::check(const QVariant& val)
{
    QString str = val.toString();
    if (!str.isEmpty() && QKeySequence(str).toString().isEmpty()) {
        return false;
    }
    return true;
}

QVariant KeySequence::fallback()
{
    return fallback_;
}

QString KeySequence::expected()
{
    return QStringLiteral("keyboard shortcut");
}

QVariant KeySequence::representation(const QVariant& val)
{
    QString str(val.toString());
    if (QKeySequence(str) == QKeySequence(Qt::Key_Return)) {
        return QStringLiteral("Enter");
    }
    return str;
}

QVariant KeySequence::process(const QVariant& val)
{
    QString str(val.toString());
    if (str == "Enter") {
        return QKeySequence(Qt::Key_Return).toString();
    }
    return str;
}
