#ifndef PROJECT_SETTINGS_H
#define PROJECT_SETTINGS_H

#include <QString>

class TabPane;

class project_settings
{
public:
    explicit project_settings(TabPane* tp);

    void title(QString t);
    const QString& title() const noexcept  { return title_;}

    void path(QString t);
    const QString& path() const noexcept  { return path_;}

    void modified(bool s);
    bool modified() const noexcept { return changed_; }

    bool isDefaultProjectName() const { return (0 == path_.compare("untitled")); }
private:
    TabPane* tp_;
    QString title_ = "untitled";
    QString path_ = "untitled";
    bool changed_ = false;
};

#endif // PROJECT_SETTINGS_H
