#include "model_thumbnail.h"

#include "model_preview.h"

#include <QColor>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QObject>
#include <QProcess>
#include <QQuickItem>
#include <QQuickWidget>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QVariant>
#include <QUrl>

int render_model_thumbnail_file(const QString& glbPath,
                                const QString& imagePath)
{
    QFile modelFile(glbPath);
    if (!modelFile.open(QIODevice::ReadOnly) || modelFile.size() <= 0
        || modelFile.size() > 128LL * 1024 * 1024) {
        return 1;
    }
    const ModelBounds bounds = model_bounds(modelFile.readAll());
    QUrl preparedSource;
    QTemporaryDir preparedDir;
    const QString balsam = QStandardPaths::findExecutable(QStringLiteral("balsam"));
    if (!balsam.isEmpty() && preparedDir.isValid()
        && QProcess::execute(balsam,
                             {QStringLiteral("-o"), preparedDir.path(), glbPath}) == 0) {
        const QStringList qmlFiles = QDir(preparedDir.path()).entryList(
            {QStringLiteral("*.qml")}, QDir::Files);
        if (qmlFiles.size() == 1) {
            preparedSource = QUrl::fromLocalFile(
                preparedDir.filePath(qmlFiles.first()));
        }
    }

    QQuickWidget thumbnail;
    thumbnail.setResizeMode(QQuickWidget::SizeRootObjectToView);
    thumbnail.resize(400, 400);
    // Qt Quick needs the widget shown to render. This flag lets it render
    // without creating an on-screen window or taking focus.
    thumbnail.setAttribute(Qt::WA_DontShowOnScreen);
    thumbnail.setAttribute(Qt::WA_ShowWithoutActivating);
    thumbnail.setSource(QUrl(QStringLiteral("qrc:/model/model_viewer.qml")));
    if (thumbnail.status() != QQuickWidget::Ready || !thumbnail.rootObject()) {
        return 1;
    }
    thumbnail.show();
    QObject* root = thumbnail.rootObject();
    root->setProperty("showHint", false);
    root->setProperty("preparedCenter", QVariant::fromValue(bounds.center));
    root->setProperty("preparedExtent", bounds.extent);
    if (preparedSource.isEmpty()) {
        root->setProperty("modelSource", QUrl::fromLocalFile(glbPath));
    } else {
        root->setProperty("preparedSource", preparedSource);
    }

    QEventLoop loop;
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (root->property("loaded").toBool()
            || !root->property("error").toString().isEmpty()) {
            loop.quit();
        }
    });
    poll.start(30);
    QTimer::singleShot(45000, &loop, &QEventLoop::quit);
    if (!root->property("loaded").toBool()) {
        loop.exec();
    }
    if (!root->property("loaded").toBool()) {
        return 1;
    }
    const QImage image = thumbnail.grabFramebuffer();
    if (image.isNull() || image.size() != QSize(400, 400)) {
        return 1;
    }
    int modelPixels = 0;
    for (int y = 0; y < image.height(); y += 4) {
        for (int x = 0; x < image.width(); x += 4) {
            modelPixels += image.pixelColor(x, y) != QColor(QStringLiteral("#28313d"));
        }
    }
    if (modelPixels < 20 || !image.save(imagePath, "PNG")) {
        return 1;
    }
    return 0;
}
