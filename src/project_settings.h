#ifndef PROJECT_SETTINGS_H
#define PROJECT_SETTINGS_H

#include <QString>

class MainWindow;

class project_settings
{
public:
    explicit project_settings(MainWindow* mw);

    void title(QString t);
    const QString& title() const noexcept  { return title_;}

    void path(QString t);
    const QString& path() const noexcept  { return path_;}

    void change(bool s);
    bool change() const noexcept { return changed_; }

    bool isDefaultProjectName() const { return (0 == title_.compare("Untitled project")); }
private:
    MainWindow* mw_;
    QString title_ = "Untitled project";
    QString path_;
    bool changed_ = false;
};

#endif // PROJECT_SETTINGS_H
