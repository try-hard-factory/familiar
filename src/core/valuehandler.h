#ifndef VALUEHANDLER_H
#define VALUEHANDLER_H

#include <QColor>
#include <QKeySequence>
#include <QList>
#include <QMap>
#include <QString>

class QVariant;

/**
 * @brief Handles the value of a configuration option (abstract class).
 *
 * Each configuration option is represented as a `QVariant`. If the option was
 * not specified in a config file, the `QVariant` will be invalid.
 *
 * Each option will usually be handled in three different ways:
 * - have its value checked for semantic errors (type, format, etc).
 *   @see ValueHandler::check
 * - have its value (that was taken from the config file) adapted for proper
 * use.
 *   @see ValueHandler::value
 * - provided a fallback value in case: the config does not explicitly specify
 *   it, or the config contains an error and is globally falling back to
 *   defaults.
 *   @see ValueHandler::fallback.
 * - some options may want to be stored in the config file in a different way
 *   than the default one provided by `QVariant`.
 *   @see ValueHandler::representation
 *
 * @note Please see the documentation of the functions to learn when you should
 * override each.
 *
 */
class ValueHandler
{
public:
    /**
     * @brief Check the value semantically.
     * @param val The value that was read from the config file
     * @return Whether the value is correct
     * @note The function should presume that `val.isValid()` is true.
     */
    virtual bool check(const QVariant& val) = 0;
    /**
     * @brief Adapt the value for proper use.
     * @param val The value that was read from the config file
     * @return The modified value
     *
     * If the value is invalid (unspecified in the config) or does not pass
     * `check`, the fallback will be returned. Otherwise the value is processed
     * by `process` and then returned.
     *
     * @note Cannot be overridden
     * @see fallback, process
     */
    QVariant value(const QVariant& val);
    /**
     * @brief Fallback value (default value).
     */
    virtual QVariant fallback();
    /**
     * @brief Return the representation of the value in the config file.
     *
     * Override this if you want to write the value in a different format than
     * the one provided by `QVariant`.
     */
    virtual QVariant representation(const QVariant& val);
    /**
     * @brief The expected value (descriptive).
     * Used when reporting configuration errors.
     */
    virtual QString expected();

protected:
    /**
     * @brief Process a value, presuming it is a valid `QVariant`.
     * @param val The value that was read from the config file
     * @return The processed value
     * @note You will usually want to override this. In rare cases, you may want
     * to override `value`.
     */
    virtual QVariant process(const QVariant& val);
};

class Bool : public ValueHandler
{
public:
    Bool(bool def);
    bool check(const QVariant& val) override;
    QVariant fallback() override;
    QString expected() override;

private:
    bool def_;
};

class BoundedInt : public ValueHandler
{
public:
    BoundedInt(int min, int max, int def);
    bool check(const QVariant& val) override;
    QVariant fallback() override;
    QString expected() override;

private:
    int m_min, m_max, m_def;
};

class KeySequence : public ValueHandler
{
public:
    KeySequence(const QKeySequence& fallback = {});
    bool check(const QVariant& val) override;
    QVariant fallback() override;
    QString expected() override;
    QVariant representation(const QVariant& val) override;

private:
    QKeySequence fallback_;

    QVariant process(const QVariant& val) override;
};

class Color : public ValueHandler
{
public:
    Color(QColor def);
    bool check(const QVariant& val) override;
    QVariant process(const QVariant& val) override;
    QVariant fallback() override;
    QVariant representation(const QVariant& val) override;
    QString expected() override;

private:
    QColor m_def;
};

class ColorList : public ValueHandler
{
public:
    ColorList(QMap<int, QColor> def);
    bool check(const QVariant& val) override;
    QVariant process(const QVariant& val) override;
    QVariant fallback() override;
    QVariant representation(const QVariant& val) override;
    QString expected() override;
private:
    QMap<int, QColor> m_def;    
};

class OpacityList : public ValueHandler
{
public:
    OpacityList(QMap<int, int> def);
    bool check(const QVariant& val) override;
    QVariant process(const QVariant& val) override;
    QVariant fallback() override;
    QVariant representation(const QVariant& val) override;
    QString expected() override;
private:
    QMap<int, int> m_def;    
};

#endif // VALUEHANDLER_H
