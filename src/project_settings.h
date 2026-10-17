#ifndef PROJECT_SETTINGS_H
#define PROJECT_SETTINGS_H

#include <QString>
#include <QUuid>

class TabPane;
class CanvasView;

class ProjectSettings
{
public:
    explicit ProjectSettings(TabPane* tp, CanvasView* view);

    void title(const QString& t);
    const QString& title() const noexcept { return title_; }

    void path(const QString& p);
    const QString& path() const noexcept { return path_; }

    void project_name(const QString& p);
    const QString& project_name() const noexcept { return projectName_; }

    void modified(bool s);
    bool modified() const noexcept { return changed_; }

    bool is_default_project_name() const
    {
        return (0 == projectName_.compare("untitled"));
    }

    // Stable for this tab's whole lifetime, regardless of path changes
    // via Save As - identifies this tab's own file(s) in the crash-
    // recovery folder (see recovery.h), which needs an identity that
    // doesn't depend on ever having a real save path.
    QUuid recovery_id() const noexcept { return recoveryId_; }

private:
    TabPane* tp_;
    CanvasView* view_;
    QString projectName_ = "untitled";
    QString title_ = "untitled";
    QString path_ = "untitled";
    bool changed_ = false;
    QUuid recoveryId_ = QUuid::createUuid();
};

#endif // PROJECT_SETTINGS_H
