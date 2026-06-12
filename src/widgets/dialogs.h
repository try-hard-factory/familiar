#pragma once

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLoggingCategory>
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>
#include <QVariantMap>

class Worker : public QThread
{
    Q_OBJECT

signals:
    void progress(int value);
    void finished(const QString& filename, const QStringList& errors);
    void beginProcessing(int value);

public:
    Worker(const std::function<void(Worker*)>& func,
           const QStringList& args,
           QVariantMap& kwargs)
        : func(func)
        , args(args)
        , kwargs(kwargs)
    {
        QString key("worker");
        kwargs.insert(key, QVariant::fromValue(this));
        canceled = false;
    }

    void run() override { func(this); }

    Q_INVOKABLE void onCanceled() { canceled = true; }

private:
    std::function<void(Worker*)> func;
    QStringList args;
    QVariantMap kwargs;
    bool canceled;
};

class ProgressDialog : public QProgressDialog
{
    Q_OBJECT

public:
    explicit ProgressDialog(const QString& label,
                               Worker* worker,
                               int maximum = 0,
                               QWidget* parent = nullptr)
        : QProgressDialog(label, QStringLiteral("Cancel"), 0, maximum, parent)
    {
        qDebug() << "Initialized progress bar";
        setMinimumDuration(0);
        setWindowModality(Qt::WindowModal);
        setAutoReset(false);
        setAutoClose(false);
        connect(worker,
                &Worker::beginProcessing,
                this,
                &ProgressDialog::on_begin_processing);
        connect(worker,
                &Worker::progress,
                this,
                &ProgressDialog::on_progress);
        connect(worker,
                &Worker::finished,
                this,
                &ProgressDialog::on_finished);
        connect(this, &ProgressDialog::canceled, worker, &Worker::onCanceled);
    }

private slots:
    void on_progress(int value)
    {
        qDebug() << "Progress dialog:" << value;
        setValue(value);
    }

    void on_begin_processing(int value)
    {
        qDebug() << "Begin progress dialog:" << value;
        setMaximum(value);
    }

    void on_finished(const QString& filename, const QStringList& errors)
    {
        qDebug() << "Finished progress dialog";
        setValue(maximum());
        reset();
        hide();
        QTimer::singleShot(100, this, &QObject::deleteLater);
    }
};


class HelpDialog : public QDialog
{
    Q_OBJECT

public:
    HelpDialog(QWidget* parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(qApp->applicationName() + " Help");
        QString docDir = QCoreApplication::applicationDirPath()
                         + "/documentation";
        QTabWidget* tabs = new QTabWidget();

        QFile controlsFile(docDir + "/controls.html");
        QString controlsTxt;
        if (controlsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&controlsFile);
            controlsTxt = in.readAll();
        }
        QLabel* controls = new QLabel(controlsTxt);
        controls->setTextInteractionFlags(Qt::TextSelectableByMouse);
        QScrollArea* scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setWidget(controls);
        tabs->addTab(scroll, "&Controls");

        QVBoxLayout* layout = new QVBoxLayout();
        setLayout(layout);
        layout->addWidget(tabs);
        show();
    }
};


class DebugLogDialog : public QDialog
{
    Q_OBJECT

public:
    DebugLogDialog(QWidget* parent)
        : QDialog(parent)
    {
        setWindowTitle(qApp->applicationName() + " Debug Log");
        QString logPath = logfileName();
        QFile file(logPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            logTxt = in.readAll();
        }

        log = new QPlainTextEdit(logTxt);
        log->setReadOnly(true);

        QDialogButtonBox* buttons = new QDialogButtonBox(
            QDialogButtonBox::Close);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        copyButton = new QPushButton("Co&py To Clipboard");
        buttons->addButton(copyButton, QDialogButtonBox::ActionRole);
        connect(copyButton,
                &QPushButton::released,
                this,
                &DebugLogDialog::copyToClipboard);

        QVBoxLayout* layout = new QVBoxLayout();
        setLayout(layout);
        QLabel* nameWidget = new QLabel(logPath);
        nameWidget->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(nameWidget);
        layout->addWidget(log);
        layout->addWidget(buttons);
        show();
    }

private:
    QPlainTextEdit* log;
    QPushButton* copyButton;
    QString logTxt;

    static QString logfileName()
    {
        return QStandardPaths::writableLocation(
                   QStandardPaths::AppLocalDataLocation)
               + "/" + qApp->applicationName() + ".log";
    }

private slots:
    void copyToClipboard()
    {
        QClipboard* clipboard = QApplication::clipboard();
        clipboard->setText(logTxt);
    }
};
