#pragma once

#include <core/settingshandler.h>
#include <utils/utils.h>
#include <QKeyEvent>
#include <QWidget>

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