#ifndef CONTROLS_H
#define CONTROLS_H

#include <QList>
#include <QMap>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <Qt>
#include <optional>

class QWheelEvent;
class QMouseEvent;

// ─── MouseConfigBase ──────────────────────────────────────────────────────────

class MouseConfigBase
{
public:
    virtual ~MouseConfigBase() = default;

    virtual const QString& id() const = 0;
    virtual const QString& group() const = 0;
    virtual const QString& text() const = 0;
    virtual const char* settingsGroup() const = 0;

    virtual bool controlsChanged() const = 0;
    virtual bool isConfigured() const = 0;
    virtual void removeControls() const = 0;

    bool isInvertible() const { return invertible_; }
    bool defaultInverted() const { return defaultInverted_; }
    const QStringList& defaultModifiers() const { return defaultModifiers_; }
    virtual QString defaultButton() const { return {}; }

    bool operator==(const MouseConfigBase& o) const { return id() == o.id(); }

    // Ordered modifier name → Qt flag mapping.
    static const QList<QPair<QString, Qt::KeyboardModifier>>& modifierMap();
    // Ordered button name → Qt flag mapping.
    static const QList<QPair<QString, Qt::MouseButton>>& buttonMap();
    // Convert list of modifier names to combined Qt::KeyboardModifiers.
    static Qt::KeyboardModifiers modifiersToQt(const QStringList& modifiers);

    QStringList getModifiers() const;
    void setModifiers(const QStringList& values) const;
    bool getInverted() const;
    void setInverted(bool value) const;

protected:
    MouseConfigBase(const QString& id, const QString& group, const QString& text,
                    const QStringList& defaultModifiers, bool invertible);

    QString id_;
    QString group_;
    QString text_;
    QStringList defaultModifiers_;
    bool invertible_;
    bool defaultInverted_ = false;
};

// ─── MouseWheelConfig ─────────────────────────────────────────────────────────

class MouseWheelConfig : public MouseConfigBase
{
public:
    MouseWheelConfig(const QString& id, const QString& group, const QString& text,
                     const QStringList& defaultModifiers, bool invertible);

    const QString& id()    const override { return id_; }
    const QString& group() const override { return group_; }
    const QString& text()  const override { return text_; }
    const char* settingsGroup() const override;

    bool controlsChanged() const override;
    bool isConfigured() const override;
    void removeControls() const override;
    bool conflictsWith(const MouseWheelConfig& other) const;
    bool matchesEvent(const QWheelEvent* event) const;
};

// ─── MouseConfig ──────────────────────────────────────────────────────────────

class MouseConfig : public MouseConfigBase
{
public:
    MouseConfig(const QString& id, const QString& group, const QString& text,
                const QString& defaultButton, const QStringList& defaultModifiers,
                bool invertible);

    const QString& id()    const override { return id_; }
    const QString& group() const override { return group_; }
    const QString& text()  const override { return text_; }
    const char* settingsGroup() const override;

    QString getButton() const;
    void setButton(const QString& value) const;
    QString defaultButton() const override { return defaultButton_; }

    bool controlsChanged() const override;
    bool isConfigured() const override;
    void removeControls() const override;
    bool conflictsWith(const MouseConfig& other) const;
    bool matchesEvent(const QMouseEvent* event) const;

private:
    QString defaultButton_;
};

// ─── KeyboardSettings ─────────────────────────────────────────────────────────

struct ControlMatch {
    QString group;
    bool inverted = false;
};

class KeyboardSettings : public QSettings
{
public:
    KeyboardSettings();

    static const QList<MouseWheelConfig>& mousewheelActions();
    static const QList<MouseConfig>& mouseActions();

    // ── Shortcut API (used by Action) ─────────────────────────────────────────
    // Saves even if equal to default (saveUnknownShortcuts flag controls this).
    void setShortcuts(const QString& group, const QString& key,
                      const QStringList& values);
    QStringList getShortcuts(const QString& group, const QString& key,
                              const QStringList& defaultValues = {});

    // ── Generic list API (used by mouse/wheel configs) ────────────────────────
    // Removes key when values == defaultValues (stores only non-default data).
    void setList(const QString& group, const QString& key,
                 const QStringList& values, const QStringList& defaultValues = {});
    QStringList getList(const QString& group, const QString& key,
                        const QStringList& defaultValues = {}) const;

    // ── Generic scalar API (used by mouse/wheel configs) ──────────────────────
    void setScalar(const QString& group, const QString& key,
                   const QVariant& value, const QVariant& defaultValue = {});
    QVariant getScalar(const QString& group, const QString& key,
                       const QVariant& defaultValue = {}) const;

    // Removes all stored controls and emits SettingsEvents::restoreKeyboardDefaults.
    void restoreDefaults();

    std::optional<ControlMatch> mousewheelActionForEvent(const QWheelEvent* event) const;
    std::optional<ControlMatch> mouseActionForEvent(const QMouseEvent* event) const;

    bool saveUnknownShortcuts = true;
};

#endif // CONTROLS_H
