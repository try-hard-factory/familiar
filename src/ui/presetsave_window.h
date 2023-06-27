#pragma once

#include <QWidget>
#include <utils/utils.h>
#include <core/settingshandler.h>
#include <QKeyEvent>

class PresetSaveWindow : public QWidget
{
    Q_OBJECT
public:
    explicit PresetSaveWindow(QWidget* parent = nullptr);
protected:
    void keyPressEvent(QKeyEvent*) override;
private:
    void savePresetToSettings(EPresets preset);
};