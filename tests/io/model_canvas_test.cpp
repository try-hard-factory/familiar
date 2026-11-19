#include "canvasscene.h"
#include "canvasview.h"
#include "fml_archive.h"
#include "mainwindow.h"
#include "modelitem.h"
#include "ui/model_toolbar.h"

#include <gtest/gtest.h>
#include <QDataStream>
#include <QFile>
#include <QQuickItem>
#include <QQuickWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QUndoStack>

TEST(ModelCanvasTest, ModelAndViewSurviveProjectRoundTrip)
{
    MainWindow window;
    QUndoStack undo;
    uint64_t zCounter = 0;
    CanvasScene source(window, zCounter, &undo);
    QImage preview(400, 400, QImage::Format_RGB32);
    preview.fill(Qt::red);
    const QByteArray bytes = QByteArrayLiteral("glTF model payload");
    auto* original = new ModelItem(preview,
                                   bytes,
                                   QStringLiteral("example.glb"));
    original->set_view(71, -23, 620);
    source.addItem(original);

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString filename = dir.filePath(QStringLiteral("model.fml"));
    const FmlResult saved = FmlArchive::save(&source, {}, filename);
    ASSERT_TRUE(saved.error.isEmpty()) << saved.error.toStdString();
    ASSERT_TRUE(saved.itemErrors.isEmpty());

    CanvasScene restored(window, zCounter, &undo);
    const FmlResult loaded = FmlArchive::load(filename, &restored);
    ASSERT_TRUE(loaded.error.isEmpty()) << loaded.error.toStdString();
    ASSERT_TRUE(loaded.itemErrors.isEmpty());
    const auto items = restored.add_queued_items();
    ASSERT_EQ(items.size(), 1);
    auto* model = dynamic_cast<ModelItem*>(items.first());
    ASSERT_NE(model, nullptr);
    EXPECT_EQ(model->model_bytes(), bytes);
    EXPECT_EQ(model->yaw(), 71);
    EXPECT_EQ(model->pitch(), -23);
    EXPECT_EQ(model->distance(), 620);
    EXPECT_EQ(model->pixmap().toImage().pixelColor(200, 200), QColor(Qt::red));
}

TEST(ModelCanvasTest, ViewerQmlLoads)
{
    QQuickWidget viewer;
    viewer.setSource(QUrl(QStringLiteral("qrc:/model/model_viewer.qml")));
    EXPECT_EQ(viewer.status(), QQuickWidget::Ready);
    ASSERT_NE(viewer.rootObject(), nullptr);
}

TEST(ModelCanvasTest, DuplicateKeepsTheModelEditable)
{
    QImage preview(400, 400, QImage::Format_RGB32);
    preview.fill(Qt::blue);
    ModelItem model(preview,
                    QByteArrayLiteral("glTF data"),
                    QStringLiteral("example.glb"));
    model.set_view(40, 20, 510);
    std::unique_ptr<IBaseItem> clone(model.create_copy());
    auto* duplicate = dynamic_cast<ModelItem*>(clone.get());
    ASSERT_NE(duplicate, nullptr);
    EXPECT_EQ(duplicate->model_bytes(), model.model_bytes());
    EXPECT_EQ(duplicate->yaw(), model.yaw());
    EXPECT_EQ(duplicate->pitch(), model.pitch());
    EXPECT_EQ(duplicate->distance(), model.distance());
}

TEST(ModelCanvasTest, RuntimeLoaderOpensGlb)
{
    QByteArray geometry;
    QDataStream binary(&geometry, QIODevice::WriteOnly);
    binary.setByteOrder(QDataStream::LittleEndian);
    binary.setFloatingPointPrecision(QDataStream::SinglePrecision);
    for (float value :
         {-1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f}) {
        binary << value;
    }
    for (quint16 index : {0, 1, 2}) {
        binary << index;
    }
    ASSERT_EQ(geometry.size(), 42);
    while (geometry.size() % 4) {
        geometry.append('\0');
    }

    QByteArray json = QByteArrayLiteral(R"({"asset":{"version":"2.0"},"scene":0,
"scenes":[{"nodes":[0]}],"nodes":[{"mesh":0}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],
"buffers":[{"byteLength":42}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36,"target":34962},
{"buffer":0,"byteOffset":36,"byteLength":6,"target":34963}],
"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3",
"min":[-1,-1,0],"max":[1,1,0]},
{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}]})");
    while (json.size() % 4) {
        json.append(' ');
    }
    QByteArray glb;
    QDataStream stream(&glb, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << quint32(0x46546C67) << quint32(2)
           << quint32(12 + 8 + json.size() + 8 + geometry.size())
           << quint32(json.size()) << quint32(0x4E4F534A);
    stream.writeRawData(json.constData(), json.size());
    stream << quint32(geometry.size()) << quint32(0x004E4942);
    stream.writeRawData(geometry.constData(), geometry.size());

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString filename = dir.filePath(QStringLiteral("triangle.glb"));
    QFile file(filename);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(file.write(glb), glb.size());
    file.close();

    QQuickWidget viewer;
    viewer.resize(400, 400);
    viewer.setSource(QUrl(QStringLiteral("qrc:/model/model_viewer.qml")));
    ASSERT_EQ(viewer.status(), QQuickWidget::Ready);
    const QString sourceFile = qEnvironmentVariable("FAMILIAR_TEST_GLB",
                                                    filename);
    viewer.rootObject()->setProperty("modelSource",
                                     QUrl::fromLocalFile(sourceFile));
    viewer.show();
    QTRY_VERIFY_WITH_TIMEOUT(viewer.rootObject()->property("loaded").toBool(),
                             5000);

    MainWindow window;
    window.show();
    CanvasView* canvas = window.tab_pane().current_widget();
    ASSERT_NE(canvas, nullptr);
    canvas->do_insert_images({QUrl::fromLocalFile(sourceFile)},
                             QPoint(160, 160));
    const auto models = canvas->scene()->items_by_type("model");
    ASSERT_EQ(models.size(), 1);
    auto* model = dynamic_cast<ModelItem*>(models.first());
    ASSERT_NE(model, nullptr);
    const QImage thumbnail = model->pixmap().toImage();
    ASSERT_FALSE(thumbnail.isNull());
    bool hasGeometry = false;
    for (int y = 0; y < thumbnail.height() && !hasGeometry; ++y) {
        for (int x = 0; x < thumbnail.width(); ++x) {
            if (thumbnail.pixelColor(x, y) != QColor("#28313d")) {
                hasGeometry = true;
                break;
            }
        }
    }
    EXPECT_TRUE(hasGeometry);
    auto* toolbar = canvas->findChild<ModelToolbar*>();
    ASSERT_NE(toolbar, nullptr);
    auto* button = toolbar->findChild<QToolButton*>();
    ASSERT_NE(button, nullptr);
    EXPECT_TRUE(model->isSelected());
    EXPECT_EQ(toolbar->item(), model);
    EXPECT_TRUE(toolbar->isVisible());
    EXPECT_FALSE(button->isChecked());
    auto* editViewer = canvas->findChild<QQuickWidget*>();
    ASSERT_NE(editViewer, nullptr);
    EXPECT_FALSE(editViewer->isVisible());

    canvas->scene()->deselect_all_items();
    QTest::mouseClick(canvas->viewport(),
                      Qt::LeftButton,
                      {},
                      canvas->mapFromScene(model->sceneBoundingRect().center()));
    EXPECT_TRUE(model->isSelected());
    EXPECT_TRUE(toolbar->isVisible());

    QTest::mouseClick(button, Qt::LeftButton);
    EXPECT_TRUE(button->isChecked());
    EXPECT_TRUE(editViewer->isVisible());
    EXPECT_TRUE(window.isVisible());

    QTest::mouseClick(button, Qt::LeftButton);
    EXPECT_FALSE(button->isChecked());
    EXPECT_FALSE(editViewer->isVisible());
    EXPECT_TRUE(model->isSelected());
}
