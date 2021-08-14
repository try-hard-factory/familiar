#ifndef PROJECT_SETTINGS_H
#define PROJECT_SETTINGS_H

#include <QString>

class MainWindow;

class project_settings
{
public:
    explicit project_settings(MainWindow* mw);
    void title(QString t) noexcept  { title_ = t; }
    const QString& title() const noexcept  { return title_;}
private:
    MainWindow* mw_;
    QString title_ = "Untitled project";
};

#endif // PROJECT_SETTINGS_H
