#include "presetsave_window.h"
#include <core/settingshandler.h>
#include <QPushButton>
#include <QVBoxLayout>

PresetSaveWindow::PresetSaveWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle(tr("Save to Preset"));
    // resize() first so geometry().height() below reports the laid-out
    // height; the width is then pinned and the height left as laid out.
    constexpr int kinitialWidth = 80;
    constexpr int kinitialHeight = 300;
    constexpr int kfixedWidth = 240;
    resize(kinitialWidth, kinitialHeight);
    setFixedSize(kfixedWidth, this->geometry().height());

    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint
                   | Qt::MSWindowsFixedSizeDialogHint);
    setWindowModality(Qt::ApplicationModal);

    auto* layout = new QVBoxLayout(this);
    // layout_->setAlignment(Qt::AlignLeft);
    auto* custom1Btn = new QPushButton("Custom 1");
    connect(custom1Btn, &QPushButton::clicked, this, [this]() {
        auto currentPreset
            = SettingsHandler::get_instance()->get_current_color_preset();
        auto currentOpacity
            = SettingsHandler::get_instance()->get_current_opacity();
        auto masterOpacity = SettingsHandler::get_instance()->master_opacity();
        masterOpacity[EPresets::Custom1] = currentOpacity;
        SettingsHandler::get_instance()->set_master_opacity(masterOpacity);
        SettingsHandler::get_instance()->set_custom_preset1(currentPreset);
        close();
    });
    auto* custom2Btn = new QPushButton("Custom 2");
    connect(custom2Btn, &QPushButton::clicked, this, [this]() {
        auto currentPreset
            = SettingsHandler::get_instance()->get_current_color_preset();
        auto currentOpacity
            = SettingsHandler::get_instance()->get_current_opacity();
        auto masterOpacity = SettingsHandler::get_instance()->master_opacity();
        masterOpacity[EPresets::Custom2] = currentOpacity;
        SettingsHandler::get_instance()->set_master_opacity(masterOpacity);
        SettingsHandler::get_instance()->set_custom_preset2(currentPreset);
        close();
    });
    auto* custom3Btn = new QPushButton("Custom 3");
    connect(custom3Btn, &QPushButton::clicked, this, [this]() {
        auto currentPreset
            = SettingsHandler::get_instance()->get_current_color_preset();
        auto currentOpacity
            = SettingsHandler::get_instance()->get_current_opacity();
        auto masterOpacity = SettingsHandler::get_instance()->master_opacity();
        masterOpacity[EPresets::Custom3] = currentOpacity;
        SettingsHandler::get_instance()->set_master_opacity(masterOpacity);
        SettingsHandler::get_instance()->set_custom_preset3(currentPreset);
        close();
    });
    auto* custom4Btn = new QPushButton("Custom 4");
    connect(custom4Btn, &QPushButton::clicked, this, [this]() {
        auto currentPreset
            = SettingsHandler::get_instance()->get_current_color_preset();
        auto currentOpacity
            = SettingsHandler::get_instance()->get_current_opacity();
        auto masterOpacity = SettingsHandler::get_instance()->master_opacity();
        masterOpacity[EPresets::Custom4] = currentOpacity;
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