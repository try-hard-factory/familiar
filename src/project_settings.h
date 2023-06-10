#ifndef PROJECT_SETTINGS_H
#define PROJECT_SETTINGS_H

#include <QString>

class TabPane;

class project_settings
{
public:
    explicit project_settings(TabPane* tp);

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

private:
    TabPane* tp_;
    QString projectName_ = "untitled";
    QString title_ = "untitled";
    QString path_ = "untitled";
    bool changed_ = false;
};

#endif // PROJECT_SETTINGS_H
