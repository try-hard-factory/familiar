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
    : mMin_(min)
    , mMax_(max)
    , mDef_(def)
{}

bool BoundedInt::check(const QVariant& val)
{
    QString str = val.toString();
    bool conversionOk;
    int num = str.toInt(&conversionOk);
    return conversionOk && mMin_ <= num && num <= mMax_;
}

QVariant BoundedInt::fallback()
{
    return mDef_;
}

QString BoundedInt::expected()
{
    return QStringLiteral("number between %1 and %2").arg(mMin_).arg(mMax_);
}


// COLOR

Color::Color(QColor def)
    : mDef_(std::move(def))
{}

bool Color::check(const QVariant& val)
{
    QString str = val.toString();
    // Disable #RGB, #RRRGGGBBB and #RRRRGGGGBBBB formats that QColor supports
    return QColor::isValidColorName(str)
           && (str[0] != '#'
               || (str.length() != 4 && str.length() != 10
                   && str.length() != 13));
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
    return mDef_;
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

ColorList::ColorList(QMap<int, QColor> def)
    : mDef_(def)
{}

bool ColorList::check([[maybe_unused]] const QVariant& val)
{
    return true;
}

// Stored as a JSON array of hex color strings, index = map key - not the
// QMap<int,QColor> itself, which only round-tripped through QSettings by
// accident (IniFormat base64-encodes unknown QVariant types via
// QDataStream, i.e. the "ini" file held opaque blobs for this key). The
// value handled internally (process()'s return, what callers actually
// get back from SettingsHandler::value()) is still QMap<int,QColor> -
// only the storage-facing shape (representation()/the input to process())
// changed.
QVariant ColorList::process(const QVariant& val)
{
    QMap<int, QColor> out;
    const QVariantList list = val.toList();
    for (int i = 0; i < list.size(); ++i) {
        out[i] = QColor(list[i].toString());
    }
    return QVariant::fromValue(out);
}

QVariant ColorList::fallback()
{
    return QVariant::fromValue(mDef_);
}

QVariant ColorList::representation(const QVariant& val)
{
    const auto map = val.value<QMap<int, QColor>>();
    QVariantList out;
    for (int i = 0; i < map.size(); ++i) {
        out.append(map.value(i).name(QColor::HexArgb));
    }
    return out;
}

QString ColorList::expected()
{
    return QStringLiteral("please don't edit by hand");
}


// OPACITY LIST

OpacityList::OpacityList(QMap<int, int> def)
    : mDef_(def)
{}

bool OpacityList::check([[maybe_unused]] const QVariant& val)
{
    return true;
}

// Same reasoning as ColorList above - JSON array of ints, index = map key.
QVariant OpacityList::process(const QVariant& val)
{
    QMap<int, int> out;
    const QVariantList list = val.toList();
    for (int i = 0; i < list.size(); ++i) {
        out[i] = list[i].toInt();
    }
    return QVariant::fromValue(out);
}

QVariant OpacityList::fallback()
{
    return QVariant::fromValue(mDef_);
}

QVariant OpacityList::representation(const QVariant& val)
{
    const auto map = val.value<QMap<int, int>>();
    QVariantList out;
    for (int i = 0; i < map.size(); ++i) {
        out.append(map.value(i));
    }
    return out;
}

QString OpacityList::expected()
{
    return QStringLiteral("please don't edit by hand");
}