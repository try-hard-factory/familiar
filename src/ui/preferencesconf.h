#ifndef PREFERENCESCONF_H
#define PREFERENCESCONF_H

#include <QWidget>

class QVBoxLayout;

class PreferencesConf : public QWidget
{
    Q_OBJECT
public:
    explicit PreferencesConf(QWidget* parent = nullptr);

public slots:
    void updateComponents();

signals:
private:
    QVBoxLayout* layout_ = nullptr;
};

#endif // PREFERENCESCONF_H
