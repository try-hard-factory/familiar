#pragma once

#include "main_controls.h"
#include "recent_files_view.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

class WelcomeOverlay : public MainControlsMixin<WelcomeOverlay, QWidget>
{
public:
    WelcomeOverlay(QWidget* parent = nullptr) : MainControlsMixin<WelcomeOverlay, QWidget>(parent)
    {
        control_target = parent;
        this->setAttribute(Qt::WA_NoSystemBackground);
        this->init_main_control();
        // Initialize your WelcomeOverlay-specific code here

        // Recent files
        files_layout = new QVBoxLayout(this);
        files_layout->addStretch(50);
        files_layout->addWidget(new QLabel("<h3>Recent Files</h3>", this));
        files_view = new RecentFilesView(this);
        files_layout->addWidget(files_view);
        files_layout->addStretch(50);

        // Help text
        QLabel* label = new QLabel(txt, this);
        label->setAlignment(Qt::AlignVCenter | Qt::AlignCenter);
        layout = new QHBoxLayout(this);
        layout->addStretch(50);
        layout->addWidget(label);
        layout->addStretch(50);
        setLayout(layout);
    }

    // Define WelcomeOverlay-specific functions here
    void show()
    {
        // files_view->update_files(BeeSettings().get_recent_files(true));
        // if (BeeSettings().get_recent_files(true).size() && layout.indexOf(&files_layout) < 0)
        // {
        //     layout.insertLayout(0, &files_layout);
        // }
        QWidget::show();
    }

private:
    QVBoxLayout* files_layout = nullptr;
    RecentFilesView* files_view;
    QHBoxLayout* layout = nullptr;
    QWidget* control_target = nullptr;

    static constexpr char txt[] = R"(
        <p>Paste or drop images here.</p>
        <p>Right-click for more options.</p>
    )";
};