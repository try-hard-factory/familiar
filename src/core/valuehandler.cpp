#include "valuehandler.h"

#include <QVariant>

QVariant ValueHandler::value(const QVariant& val)
{
    if (!val.isValid() || !check(val)) {
        qDebug() << "ValueHandler::value, fallback " << val;
        return fallback();
    } else {
        qDebug() << "ValueHandler::value, process " << val;
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

// BOOL

Bool::Bool(bool def)
    : def_(def)
{}

bool Bool::check(const QVariant& val)
{
    QString str = val.toString();
    if (str != "true" && str != "false") {
        return false;
    }
    return true;
}

QVariant Bool::fallback()
{
    return def_;
}

QString Bool::expected()
{
    return QStringLiteral("true or false");
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


// BOUNDED INT

BoundedInt::BoundedInt(int min, int max, int def)
    : m_min(min)
    , m_max(max)
    , m_def(def)
{}

bool BoundedInt::check(const QVariant& val)
{
    QString str = val.toString();
    bool conversionOk;
    int num = str.toInt(&conversionOk);
    return conversionOk && m_min <= num && num <= m_max;
}

QVariant BoundedInt::fallback()
{
    return m_def;
}

QString BoundedInt::expected()
{
    return QStringLiteral("number between %1 and %2").arg(m_min).arg(m_max);
}


// COLOR

Color::Color(QColor def)
    : m_def(std::move(def))
{}

bool Color::check(const QVariant& val)
{
    QString str = val.toString();
    // Disable #RGB, #RRRGGGBBB and #RRRRGGGGBBBB formats that QColor supports
    return QColor::isValidColor(str)
           && (str[0] != '#' || (str.length() != 4 && str.length() != 10 && str.length() != 13));
}

QVariant Color::process(const QVariant& val)
{
    QString str = val.toString();
    QColor color(str);
    if (str.length() == 9 && str[0] == '#') {
        // Convert #RRGGBBAA (flameshot) to #AARRGGBB (QColor)
        int blue = color.blue();
        color.setBlue(color.green());
        color.setGreen(color.red());
        color.setRed(color.alpha());
        color.setAlpha(blue);
    }
    return color;
}

QVariant Color::fallback()
{
    return m_def;
}

QVariant Color::representation(const QVariant& val)
{
    QString str = val.toString();
    QColor color(str);
    if (str.length() == 9 && str[0] == '#') {
        // Convert #AARRGGBB (QColor) to #RRGGBBAA (flameshot)
        int alpha = color.alpha();
        color.setAlpha(color.red());
        color.setRed(color.green());
        color.setGreen(color.blue());
        color.setBlue(alpha);
    }
    return color.name();
}

QString Color::expected()
{
    return QStringLiteral("color name or hex value");
}


// COLOR LIST
using CCollection = QMap<int, QColor>;

ColorList::ColorList(QMap<int, QColor> def)
    : m_def(def)
{
    qDebug() << "ColorList::ColorList. SIZE= "<< m_def.size();
}

bool ColorList::check(const QVariant& val)
{
    return true;
}

QVariant ColorList::process(const QVariant& val)
{
    auto colorList = val.value<QMap<int, QColor>>();
    return QVariant::fromValue(colorList);
}

QVariant ColorList::fallback()
{
    return QVariant::fromValue(m_def);
}



QVariant ColorList::representation(const QVariant& val)
{
    auto colorVector = val.value<CCollection>();
    return QVariant::fromValue(colorVector);
}

QString ColorList::expected()
{
    return QStringLiteral("please don't edit by hand");
}