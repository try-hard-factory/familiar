#pragma once

#include <QProgressDialog>
#include <QLoggingCategory>
#include <QTimer>
#include <QThread>
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
    Worker(const std::function<void(Worker*)>& func, const QStringList& args, QVariantMap& kwargs)
        : func(func), args(args), kwargs(kwargs)
    {
        QString key("worker");
        kwargs.insert(key, QVariant::fromValue(this));
        canceled = false;
    }

    void run() override
    {
        func(this);
    }

    Q_INVOKABLE void onCanceled()
    {
        canceled = true;
    }

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
    explicit ProgressDialog(const QString& label, Worker* worker, int maximum = 0, QWidget* parent = nullptr)
        : QProgressDialog(label, QStringLiteral("Cancel"), 0, maximum, parent)
    {
        qDebug() << "Initialized progress bar";
        setMinimumDuration(0);
        setWindowModality(Qt::WindowModal);
        setAutoReset(false);
        setAutoClose(false);
        connect(worker, &Worker::beginProcessing, this, &ProgressDialog::on_begin_processing);
        connect(worker, &Worker::progress, this, &ProgressDialog::on_progress);
        connect(worker, &Worker::finished, this, &ProgressDialog::on_finished);
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

    void on_finished(const QString& filename, const QString& errors)
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
    HelpDialog(QWidget* parent = nullptr) : QDialog(parent)
    {
        // setWindowTitle(Constants::APPNAME + " Help");
        // QString docDir = QDir::currentPath() + "/documentation";
        // QTabWidget* tabs = new QTabWidget();

        // // Controls tab
        // QString controlsFile = docDir + "/controls.html";
        // QString controlsTxt = readFile(controlsFile);
        // QTextBrowser* controlsBrowser = new QTextBrowser();
        // controlsBrowser->setText(controlsTxt);
        // tabs->addTab(controlsBrowser, "&Controls");

        // QVBoxLayout* layout = new QVBoxLayout();
        // setLayout(layout);
        // layout->addWidget(tabs);
        // show();
    }

private:
    QString readFile(const QString& fileName)
    {
        // QFile file(fileName);
        // if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        // {
        //     QTextStream in(&file);
        //     return in.readAll();
        // }
        return QString();
    }
};

class DebugLogDialog : public QDialog
{
    Q_OBJECT

public:
    DebugLogDialog(QWidget* parent) : QDialog(parent)
    {
        // setWindowTitle(Constants::APPNAME + " Debug Log");
        // QString logTxt = readFile(logfileName());

        // log = new QPlainTextEdit(logTxt);
        // log->setReadOnly(true);

        // QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
        // connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        // copyButton = new QPushButton("Co&py To Clipboard");
        // buttons->addButton(copyButton, QDialogButtonBox::ActionRole);
        // connect(copyButton, &QPushButton::released, this, &DebugLogDialog::copyToClipboard);

        // QVBoxLayout* layout = new QVBoxLayout();
        // setLayout(layout);
        // QLabel* nameWidget = new QLabel(logfileName());
        // nameWidget->setTextInteractionFlags(Qt::TextSelectableByMouse);
        // layout->addWidget(nameWidget);
        // layout->addWidget(log);
        // layout->addWidget(buttons);
        // show();
    }

private:
    // QPlainTextEdit* log;
    QPushButton* copyButton;

    QString readFile(const QString& fileName)
    {
        // QFile file(fileName);
        // if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        // {
        //     QTextStream in(&file);
        //     return in.readAll();
        // }
        return QString();
    }

private slots:
    void copyToClipboard()
    {
        // QClipboard* clipboard = QApplication::clipboard();
        // clipboard->setText(log->toPlainText());
    }
};