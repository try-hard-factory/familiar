#include "saveallwindow.h"
#include "mainwindow.h"

SaveAllWindow::SaveAllWindow(MainWindow* wm,
                             std::map<int, QString> items,
                             QWidget* parent)
    : QWidget(parent)
    , window_(wm)
{
    setWindowModality(Qt::ApplicationModal);
    setWindowFlags(Qt::Tool | Qt::Dialog);
    resize(320, 240);

    vlayout_ = new QVBoxLayout;
    vlayout_->setContentsMargins(0, 0, 0, 0);
    setLayout(vlayout_);

    toplabel_ = new QLabel;
    toplabel_->setText("There are some docs with unsaved changes.\nSave "
                       "changes before closing?");
    vlayout_->addWidget(toplabel_);

    midlabel_ = new QLabel;
    midlabel_->setText("Select the documents you want to save:");
    vlayout_->addWidget(midlabel_);

    for (auto& [id, path] : items) {
        SaveCheckBox* chbox = new SaveCheckBox(id);
        chbox->setText(path);
        chbox->setChecked(true);
        savecheckboxMap_.emplace(id, chbox);
        vlayout_->addWidget(chbox);
        connect(chbox, SIGNAL(clicked()), this, SLOT(onSaveBoxToggled()));
    }


    hlayout_ = new QHBoxLayout;
    closeWS_btn = new QPushButton;
    closeWS_btn->setText("close without save");
    connect(closeWS_btn,
            SIGNAL(clicked()),
            this,
            SLOT(onCloseWithoutSaveClicked()));
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

    centered_widget(window_, this);
}

SaveAllWindow::~SaveAllWindow()
{
    qDebug() << "Save all window destructor";
    delete save_btn;
    delete cancel_btn;
    delete closeWS_btn;

    for (auto& [_, chbox] : savecheckboxMap_) {
        delete chbox;
    }

    delete hlayout_;
    delete toplabel_;
    delete midlabel_;
    delete vlayout_;
}

void SaveAllWindow::onCloseWithoutSaveClicked()
{
    qDebug() << "onCloseWithoutSaveClicked";
    window_->exitProject();
}

void SaveAllWindow::onCancelClicked()
{
    qDebug() << "onCancelClicked";
    this->close();
}

void SaveAllWindow::onSaveClicked()
{
    qDebug() << "onSaveClicked";
    std::map<int, bool> m;
    for (auto& [id, chbox] : savecheckboxMap_) {
        qDebug() << id << " " << chbox->text() << " " << chbox->isChecked();
        m.emplace(id, chbox->isChecked());
    }
    window_->saveAllWindowSaveCB(this, std::move(m));
}

void SaveAllWindow::onSaveBoxToggled()
{
    SaveCheckBox* chbox = (SaveCheckBox*) sender();
    qDebug() << "TOGGLED: " << chbox->id() << ", state: " << chbox->isChecked();
}
