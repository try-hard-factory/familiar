#ifndef PROJECT_SETTINGS_H
#define PROJECT_SETTINGS_H

#include <QString>
#include <QUuid>

class TabPane;
class CanvasView;

class project_settings
{
public:
    explicit project_settings(TabPane* tp, CanvasView* view);

    void title(const QString& t);
    const QString& title() const noexcept { return title_; }

    void path(const QString& p);
    const QString& path() const noexcept { return path_; }

    void projectName(const QString& p);
    const QString& projectName() const noexcept { return projectName_; };

    void modified(bool s);
    bool modified() const noexcept { return changed_; }

    bool isDefaultProjectName() const
    {
        return (0 == projectName_.compare("untitled"));
    }

    // Stable for this tab's whole lifetime, regardless of path changes
    // via Save As - identifies this tab's own file(s) in the crash-
    // recovery folder (see recovery.h), which needs an identity that
    // doesn't depend on ever having a real save path.
    QUuid recoveryId() const noexcept { return recoveryId_; }

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
