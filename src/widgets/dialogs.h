#pragma once

#include <QApplication>
#include <QBrush>
#include <QClipboard>
#include <QColor>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QGraphicsItem>
#include <QGridLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QMap>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSize>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QVariant>
#include <QVariantMap>

#include "commands.h"
#include "fileio.h"

class ProgressDialog : public QProgressDialog
{
    Q_OBJECT

public:
    explicit ProgressDialog(const QString& label,
                               ThreadedIO* worker,
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
                &ThreadedIO::beginProcessing,
                this,
                &ProgressDialog::on_begin_processing);
        connect(worker,
                &ThreadedIO::progress,
                this,
                &ProgressDialog::on_progress);
        connect(worker,
                &ThreadedIO::finished,
                this,
                &ProgressDialog::on_finished);
        connect(worker,
                &ThreadedIO::userInputRequired,
                this,
                [this](const QString&) { on_finished(QString(), {}); });
        connect(this, &ProgressDialog::canceled, worker, &ThreadedIO::onCanceled);
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

        QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);

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


class SceneToPixmapExporterDialog : public QDialog
{
    Q_OBJECT

    static constexpr int MIN_SIZE = 10;
    static constexpr int MAX_SIZE = 100000;

public:
    SceneToPixmapExporterDialog(QWidget* parent, QSize defaultSize)
        : QDialog(parent)
        , defaultSize(defaultSize)
        , ignoreChange(false)
    {
        if (defaultSize.width() > MAX_SIZE || defaultSize.width() >= MAX_SIZE)
            defaultSize.scale(MAX_SIZE, MAX_SIZE, Qt::KeepAspectRatio);

        setWindowTitle("Export Scene to Image");
        setWindowModality(Qt::WindowModal);
        QGridLayout* layout = new QGridLayout();
        setLayout(layout);

        layout->addWidget(new QLabel("Width:"), 0, 0);
        widthInput = new QSpinBox();
        widthInput->setRange(MIN_SIZE, MAX_SIZE);
        widthInput->setValue(defaultSize.width());
        connect(widthInput, &QSpinBox::valueChanged, this, &SceneToPixmapExporterDialog::onWidthChanged);
        layout->addWidget(widthInput, 0, 1);

        layout->addWidget(new QLabel("Height:"), 1, 0);
        heightInput = new QSpinBox();
        heightInput->setRange(MIN_SIZE, MAX_SIZE);
        heightInput->setValue(defaultSize.height());
        connect(heightInput, &QSpinBox::valueChanged, this, &SceneToPixmapExporterDialog::onHeightChanged);
        layout->addWidget(heightInput, 1, 1);

        QDialogButtonBox* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons, 3, 1);
    }

    QSize value() const
    {
        return QSize(widthInput->value(), heightInput->value());
    }

private slots:
    void onWidthChanged(int width)
    {
        if (!ignoreChange) {
            ignoreChange = true;
            QSize scaled = defaultSize.scaled(width, MAX_SIZE, Qt::KeepAspectRatio);
            heightInput->setValue(scaled.height());
            ignoreChange = false;
        }
    }

    void onHeightChanged(int height)
    {
        if (!ignoreChange) {
            ignoreChange = true;
            QSize scaled = defaultSize.scaled(MAX_SIZE, height, Qt::KeepAspectRatio);
            widthInput->setValue(scaled.width());
            ignoreChange = false;
        }
    }

private:
    QSize defaultSize;
    bool ignoreChange;
    QSpinBox* widthInput;
    QSpinBox* heightInput;
};


class ChangeOpacityDialog : public QDialog
{
    Q_OBJECT

public:
    ChangeOpacityDialog(QWidget* parent,
                        const QList<QGraphicsItem*>& items,
                        QUndoStack* undoStack)
        : QDialog(parent)
        , items(items)
        , undoStack(undoStack)
        , command(new ChangeOpacityCommand(items, 1.0))
    {
        int value = !items.isEmpty() ? int(items[0]->opacity() * 100) : 100;

        setWindowTitle("Change Opacity:");
        setWindowModality(Qt::WindowModal);
        QVBoxLayout* layout = new QVBoxLayout();
        setLayout(layout);

        label = new QLabel("Opacity:");
        layout->addWidget(label);

        input = new QSlider(Qt::Horizontal);
        input->setRange(0, 100);
        connect(input, &QSlider::valueChanged, this, &ChangeOpacityDialog::onValueChanged);
        input->setValue(value);
        layout->addWidget(input);

        QDialogButtonBox* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &ChangeOpacityDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &ChangeOpacityDialog::reject);
        layout->addWidget(buttons);

        show();
    }

    ~ChangeOpacityDialog() { delete command; }

public slots:
    void accept() override
    {
        if (!items.isEmpty()) {
            command->setIgnoreFirstRedo(true);
            undoStack->push(command);
            command = nullptr;
        }
        QDialog::accept();
    }

    void reject() override
    {
        if (command) {
            command->undo();
            delete command;
            command = nullptr;
        }
        QDialog::reject();
    }

private slots:
    void onValueChanged(int value)
    {
        label->setText(QString("Opacity: %1%").arg(value));
        command->setOpacity(value / 100.0);
        command->redo();
    }

private:
    QList<QGraphicsItem*> items;
    QUndoStack* undoStack;
    ChangeOpacityCommand* command;
    QLabel* label;
    QSlider* input;
};


class FamNotification : public QWidget
{
    Q_OBJECT

public:
    FamNotification(QWidget* parent, const QString& text)
        : QWidget(parent)
    {
        QLabel* label = new QLabel(text);
        setObjectName("FamNotification");
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAutoFillBackground(true);
        QVBoxLayout* layout = new QVBoxLayout();
        layout->addWidget(label);
        setLayout(layout);

        QColor color = QApplication::palette().color(QPalette::Window);
        setStyleSheet(
            QString("background-color: rgba(%1, %2, %3, 0.9); padding: 0.7em; border-radius: 5px;")
                .arg(color.red()).arg(color.green()).arg(color.blue()));

        show();
        int x = (parent->width() - width()) / 2;
        move(x, 10);

        QTimer::singleShot(1000 * 3, this, &QObject::deleteLater);
    }
};


class SampleColorWidget : public QWidget
{
    Q_OBJECT

    static constexpr int OFFSET = 10;
    static constexpr int SIZE = 50;

public:
    SampleColorWidget(QWidget* parent, const QPointF& pos, const QColor& color = QColor())
        : QWidget(parent)
        , m_color(color)
    {
        setFixedSize(SIZE, SIZE);
        setPos(pos);
        show();
    }

    void setPos(const QPointF& pos)
    {
        move(int(pos.x() + OFFSET), int(pos.y() + OFFSET));
    }

    void update(const QPointF& pos, const QColor& color)
    {
        setPos(pos);
        m_color = color;
        repaint();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QColor color = m_color.isValid() ? m_color : QColor(0, 0, 0, 0);
        QPainter painter(this);
        painter.setBrush(QBrush(color));
        painter.setPen(Qt::NoPen);
        painter.drawRect(0, 0, SIZE, SIZE);
    }

private:
    QColor m_color;
};


class ExportImagesFileExistsDialog : public QDialog
{
    Q_OBJECT

public:
    ExportImagesFileExistsDialog(QWidget* parent, const QString& filename)
        : QDialog(parent)
    {
        setWindowTitle("File exists");

        QVBoxLayout* layout = new QVBoxLayout();
        setLayout(layout);

        layout->addWidget(new QLabel(QString("File already exists:\n%1").arg(filename)));

        const QList<QPair<QString, QString>> choices = {
            {"skip",         "Skip this file"},
            {"skip_all",     "Skip all existing files"},
            {"overwrite",    "Overwrite this file"},
            {"overwrite_all","Overwrite all existing files"},
        };

        for (const auto& [value, label] : choices) {
            auto* btn = new QRadioButton(label);
            radioButtons.insert(value, btn);
            layout->addWidget(btn);
        }
        radioButtons["skip"]->setChecked(true);

        QDialogButtonBox* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);
    }

    QString getAnswer() const
    {
        for (auto it = radioButtons.constBegin(); it != radioButtons.constEnd(); ++it) {
            if (it.value()->isChecked())
                return it.key();
        }
        return "skip";
    }

private:
    QMap<QString, QRadioButton*> radioButtons;
};
