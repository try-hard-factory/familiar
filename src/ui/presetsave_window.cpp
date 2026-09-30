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
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint
                   | Qt::MSWindowsFixedSizeDialogHint);
    setWindowModality(Qt::ApplicationModal);

    auto* layout = new QVBoxLayout(this);
    // layout_->setAlignment(Qt::AlignLeft);
    QPushButton* custom1Btn = new QPushButton("Custom 1");
    connect(custom1Btn, &QPushButton::clicked, this, [this]() {
        auto currentPreset
            = SettingsHandler::get_instance()->get_current_color_preset();
        auto currentOpacity
            = SettingsHandler::get_instance()->get_current_opacity();
        auto masterOpacity = SettingsHandler::get_instance()->master_opacity();
        masterOpacity[kCustom1] = currentOpacity;
        SettingsHandler::get_instance()->set_master_opacity(masterOpacity);
        SettingsHandler::get_instance()->set_custom_preset1(currentPreset);
        close();
    });
    QPushButton* custom2Btn = new QPushButton("Custom 2");
    connect(custom2Btn, &QPushButton::clicked, this, [this]() {
        auto currentPreset
            = SettingsHandler::get_instance()->get_current_color_preset();
        auto currentOpacity
            = SettingsHandler::get_instance()->get_current_opacity();
        auto masterOpacity = SettingsHandler::get_instance()->master_opacity();
        masterOpacity[kCustom2] = currentOpacity;
        SettingsHandler::get_instance()->set_master_opacity(masterOpacity);
        SettingsHandler::get_instance()->set_custom_preset2(currentPreset);
        close();
    });
    QPushButton* custom3Btn = new QPushButton("Custom 3");
    connect(custom3Btn, &QPushButton::clicked, this, [this]() {
        auto currentPreset
            = SettingsHandler::get_instance()->get_current_color_preset();
        auto currentOpacity
            = SettingsHandler::get_instance()->get_current_opacity();
        auto masterOpacity = SettingsHandler::get_instance()->master_opacity();
        masterOpacity[kCustom3] = currentOpacity;
        SettingsHandler::get_instance()->set_master_opacity(masterOpacity);
        SettingsHandler::get_instance()->set_custom_preset3(currentPreset);
        close();
    });
    QPushButton* custom4Btn = new QPushButton("Custom 4");
    connect(custom4Btn, &QPushButton::clicked, this, [this]() {
        auto currentPreset
            = SettingsHandler::get_instance()->get_current_color_preset();
        auto currentOpacity
            = SettingsHandler::get_instance()->get_current_opacity();
        auto masterOpacity = SettingsHandler::get_instance()->master_opacity();
        masterOpacity[kCustom4] = currentOpacity;
        SettingsHandler::get_instance()->set_master_opacity(masterOpacity);
        SettingsHandler::get_instance()->set_custom_preset4(currentPreset);
        close();
    });
    layout->addWidget(custom1Btn);
    layout->addWidget(custom2Btn);
    layout->addWidget(custom3Btn);
    layout->addWidget(custom4Btn);
    setLayout(layout);
}

void PresetSaveWindow::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) {
        close();
    }
}