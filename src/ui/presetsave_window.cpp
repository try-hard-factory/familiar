#include "presetsave_window.h"
#include <core/settingshandler.h>
#include <QPushButton>
#include <QVBoxLayout>

PresetSaveWindow::PresetSaveWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle(tr("Save to Preset"));
    resize(80, 300);
    setFixedSize(240, this->geometry().height());

    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint | Qt::MSWindowsFixedSizeDialogHint);
    setWindowModality(Qt::ApplicationModal);

    auto* layout_ = new QVBoxLayout(this);
    // layout_->setAlignment(Qt::AlignLeft);
    QPushButton* custom1_btn = new QPushButton("Custom 1");
    connect(custom1_btn, &QPushButton::clicked, this, [this]() {
        auto current_preset = SettingsHandler::getInstance()->getCurrentColorPreset();
        auto current_opacity = SettingsHandler::getInstance()->getCurrentOpacity();
        auto master_opacity = SettingsHandler::getInstance()->masterOpacity();
        master_opacity[kCustom1] = current_opacity;
        SettingsHandler::getInstance()->setMasterOpacity(master_opacity);
        SettingsHandler::getInstance()->setCustomPreset1(current_preset);
        close();
    });
    QPushButton* custom2_btn = new QPushButton("Custom 2");
    connect(custom2_btn, &QPushButton::clicked, this, [this]() {
        auto current_preset = SettingsHandler::getInstance()->getCurrentColorPreset();
        auto current_opacity = SettingsHandler::getInstance()->getCurrentOpacity();
        auto master_opacity = SettingsHandler::getInstance()->masterOpacity();
        master_opacity[kCustom2] = current_opacity;
        SettingsHandler::getInstance()->setMasterOpacity(master_opacity);
        SettingsHandler::getInstance()->setCustomPreset2(current_preset);
        close();
    });
    QPushButton* custom3_btn = new QPushButton("Custom 3");
    connect(custom3_btn, &QPushButton::clicked, this, [this]() {
        auto current_preset = SettingsHandler::getInstance()->getCurrentColorPreset();
        auto current_opacity = SettingsHandler::getInstance()->getCurrentOpacity();
        auto master_opacity = SettingsHandler::getInstance()->masterOpacity();
        master_opacity[kCustom3] = current_opacity;
        SettingsHandler::getInstance()->setMasterOpacity(master_opacity);
        SettingsHandler::getInstance()->setCustomPreset3(current_preset);
        close();
    });
    QPushButton* custom4_btn = new QPushButton("Custom 4");
    connect(custom4_btn, &QPushButton::clicked, this, [this]() {
        auto current_preset = SettingsHandler::getInstance()->getCurrentColorPreset();
        auto current_opacity = SettingsHandler::getInstance()->getCurrentOpacity();
        auto master_opacity = SettingsHandler::getInstance()->masterOpacity();
        master_opacity[kCustom4] = current_opacity;
        SettingsHandler::getInstance()->setMasterOpacity(master_opacity);
        SettingsHandler::getInstance()->setCustomPreset4(current_preset);
        close();
    });
    layout_->addWidget(custom1_btn);
    layout_->addWidget(custom2_btn);
    layout_->addWidget(custom3_btn);
    layout_->addWidget(custom4_btn);
    setLayout(layout_);
}

void PresetSaveWindow::savePresetToSettings(EPresets preset) {}


void PresetSaveWindow::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) {
        close();
    }
}