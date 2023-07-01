#ifndef MAINMENUBAR_H
#define MAINMENUBAR_H

#include <QFrame>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QToolButton>
#include <core/settingshandler.h>

class MainMenuBar : public QMenuBar
{
public:
    MainMenuBar(QWidget* parent = nullptr)
        : QMenuBar(parent)
    {
        // Создание кнопок для минимизации, максимизации и закрытия
        minimizeButton_ = new QToolButton(this);
        minimizeButton_->setIcon(QIcon("://images/minimize-50.png"));
        connect(minimizeButton_, &QToolButton::clicked, this, &MainMenuBar::minimizeWindow);

        maximizeButton_ = new QToolButton(this);
        maximizeButton_->setIcon(QIcon("://images/maximize-50.png"));
        connect(maximizeButton_, &QToolButton::clicked, this, &MainMenuBar::maximizeWindow);

        closeButton_ = new QToolButton(this);
        closeButton_->setIcon(QIcon("://images/close-50.png"));
        connect(closeButton_, &QToolButton::clicked, this, &MainMenuBar::closeWindow);

        // Добавление кнопок на QMenuBar
        QWidget* emptyWidget = new QWidget(this);
        emptyWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        QLayout* layout = new QHBoxLayout;
        layout->setSpacing(0);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(emptyWidget);
        layout->addWidget(minimizeButton_);
        layout->addWidget(maximizeButton_);
        layout->addWidget(closeButton_);

        QFrame* frame = new QFrame(this);
        frame->setObjectName("MenuBarFrame");
        frame->setLayout(layout);

        setCornerWidget(frame, Qt::TopRightCorner);

        connect(SettingsHandler::getInstance(),
                &SettingsHandler::settingsChanged,
                this,
                &MainMenuBar::settingsChangedSlot);
        settingsChangedSlot();
    }

public slots:
    void settingsChangedSlot()
    {
        auto settings = SettingsHandler::getInstance();
        auto colorPreset = settings->getCurrentColorPreset();
        menuColor_ = colorPreset[EPresetsColorIdx::kMenuColor];
        //fileMenu_->setStyleSheet("background: transparent; background-color: rgba(0, 255, 0, 255);");
        QString rgbaColor = QString("rgba(%1, %2, %3, %4);")
                                .arg(menuColor_.red())
                                .arg(menuColor_.green())
                                .arg(menuColor_.blue())
                                .arg(255);
        setStyleSheet("background: transparent; background-color: " + rgbaColor);
    }

private slots:
    void minimizeWindow()
    {
        // Минимизация окна
        if (window())
            window()->showMinimized();
    }

    void maximizeWindow()
    {
        // Максимизация окна
        if (window()) {
            if (window()->isMaximized())
                window()->showNormal();
            else
                window()->showMaximized();
        }
    }

    void closeWindow()
    {
        // Закрытие окна
        if (window())
            window()->close();
    }

private:
    QToolButton* minimizeButton_ = nullptr;
    QToolButton* maximizeButton_ = nullptr;
    QToolButton* closeButton_ = nullptr;
    QColor menuColor_;
};


#endif // MAINMENUBAR_H
