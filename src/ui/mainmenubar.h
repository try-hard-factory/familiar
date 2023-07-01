#ifndef MAINMENUBAR_H
#define MAINMENUBAR_H

#include <QMenuBar>
#include <QToolButton>
#include <QHBoxLayout>
#include <QFrame>

class MainMenuBar : public QMenuBar
{
public:
    MainMenuBar(QWidget* parent = nullptr) : QMenuBar(parent)
    {
        // Создание кнопок для минимизации, максимизации и закрытия
        minimizeButton_ = new QToolButton(this);
        minimizeButton_->setIcon(QIcon(":/path/to/minimize/icon.png"));
        connect(minimizeButton_, &QToolButton::clicked, this, &MainMenuBar::minimizeWindow);

        maximizeButton_ = new QToolButton(this);
        maximizeButton_->setIcon(QIcon(":/path/to/maximize/icon.png"));
        connect(maximizeButton_, &QToolButton::clicked, this, &MainMenuBar::maximizeWindow);

        closeButton_ = new QToolButton(this);
        closeButton_->setIcon(QIcon(":/path/to/close/icon.png"));
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
        if (window())
        {
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
    QToolButton* minimizeButton_;
    QToolButton* maximizeButton_;
    QToolButton* closeButton_;
};


#endif // MAINMENUBAR_H
