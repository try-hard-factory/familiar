#include "saveallwindow.h"
#include "mainwindow.h"

SaveAllWindow::SaveAllWindow(MainWindow *wm, QWidget *parent)
    : QWidget(parent), window_(wm)
{
    setWindowModality(Qt::ApplicationModal);
    setWindowFlags(Qt::Tool | Qt::Dialog);
    resize(320, 240);

    vlayout_ = new QVBoxLayout;
    vlayout_->setContentsMargins(0,0,0,0);
    setLayout(vlayout_);
    label_ = new QLabel;
    label_->setText("TEST!");
    vlayout_->addWidget(label_);

    hlayout_ = new QHBoxLayout;
    closeWS_btn = new QPushButton;
    closeWS_btn->setText("close without save");
    connect(closeWS_btn, SIGNAL(clicked()), this, SLOT(onCloseWithoutSaveClicked()));
    cancel_btn = new QPushButton;
    cancel_btn->setText("cancel");
        connect(cancel_btn, SIGNAL(clicked()), this, SLOT(onCancelClicked()));
    save_btn = new QPushButton;
    save_btn->setText("save");
        connect(save_btn, SIGNAL(clicked()), this, SLOT(onSaveClicked()));
    hlayout_->addWidget(closeWS_btn);
    hlayout_->addWidget(cancel_btn);
    hlayout_->addWidget(save_btn);

    vlayout_->addLayout(hlayout_);

    auto hostRect = window_->geometry();
    qDebug()<<size();
    auto point = hostRect.center() - QPoint(size().width()/2,
                                            size().height()/2);
    move(point);
}

SaveAllWindow::~SaveAllWindow()
{
    delete save_btn;
    delete cancel_btn;
    delete closeWS_btn;

    delete hlayout_;
    delete label_;
    delete vlayout_;
}

void SaveAllWindow::onCloseWithoutSaveClicked()
{
    qDebug()<<"onCloseWithoutSaveClicked";
}

void SaveAllWindow::onCancelClicked()
{
    qDebug()<<"onCancelClicked";
}

void SaveAllWindow::onSaveClicked()
{
    qDebug()<<"onSaveClicked";
}
