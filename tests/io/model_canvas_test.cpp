#include "canvasscene.h"
#include "canvasview.h"
#include "fml_archive.h"
#include "gltf_import.h"
#include "obj_import.h"
#include "mainwindow.h"
#include "modelitem.h"
#include "model_preview.h"
#include "ui/model_toolbar.h"

#include <gtest/gtest.h>
#include <QDataStream>
#include <QBuffer>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QQuickItem>
#include <QQuickWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <QUndoStack>
#include <cstring>

namespace {
class EventLoopHeartbeat
{
public:
    EventLoopHeartbeat()
    {
        clock_.start();
        QObject::connect(&timer_, &QTimer::timeout, [&] {
            const qint64 now = clock_.elapsed();
            longestGap_ = std::max(longestGap_, now - previous_);
            previous_ = now;
        });
        timer_.start(20);
    }

    qint64 longest_gap() const { return longestGap_; }

private:
    QElapsedTimer clock_;
    QTimer timer_;
    qint64 previous_ = 0;
    qint64 longestGap_ = 0;
};
} // namespace

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
    EXPECT_TRUE(viewer.rootObject()->property("error").toString().isEmpty());

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString previewPath = dir.filePath(QStringLiteral("preview.png"));
    QImage preview(400, 400, QImage::Format_RGB32);
    preview.fill(Qt::red);
    ASSERT_TRUE(preview.save(previewPath));
    viewer.resize(400, 400);
    viewer.rootObject()->setProperty("previewSource",
                                     QUrl::fromLocalFile(previewPath));
    viewer.rootObject()->setProperty("preparing", true);
    viewer.show();
    auto* previewItem = viewer.rootObject()->findChild<QQuickItem*>(
        QStringLiteral("modelPreview"));
    ASSERT_NE(previewItem, nullptr);
    QTRY_COMPARE(previewItem->property("status").toInt(), 1); // Image.Ready
    EXPECT_TRUE(previewItem->isVisible());
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

TEST(ModelCanvasTest, PreviewKeepsTextureUpright)
{
    QImage texture(4, 4, QImage::Format_RGB32);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            texture.setPixelColor(x, y,
                                  y < 2 ? (x < 2 ? Qt::red : Qt::green)
                                        : (x < 2 ? Qt::blue : Qt::yellow));
        }
    }
    QByteArray png;
    QBuffer pngBuffer(&png);
    ASSERT_TRUE(pngBuffer.open(QIODevice::WriteOnly));
    ASSERT_TRUE(texture.save(&pngBuffer, "PNG"));
    pngBuffer.close();

    QByteArray binary;
    QDataStream mesh(&binary, QIODevice::WriteOnly);
    mesh.setByteOrder(QDataStream::LittleEndian);
    mesh.setFloatingPointPrecision(QDataStream::SinglePrecision);
    for (float coordinate : {-1.0f, 1.0f, 0.0f,
                             1.0f, 1.0f, 0.0f,
                             1.0f, -1.0f, 0.0f,
                             -1.0f, -1.0f, 0.0f}) {
        mesh << coordinate;
    }
    for (float uv : {0.0f, 0.0f, 1.0f, 0.0f,
                     1.0f, 1.0f, 0.0f, 1.0f}) {
        mesh << uv;
    }
    for (quint16 index : {0, 1, 2, 0, 2, 3}) {
        mesh << index;
    }
    ASSERT_EQ(binary.size(), 92);
    binary.append(png);
    const int bufferSize = binary.size();
    while (binary.size() % 4) {
        binary.append('\0');
    }

    QByteArray json = QByteArrayLiteral(R"({
"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],
"nodes":[{"mesh":0}],"meshes":[{"primitives":[{
"attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2,"material":0}]}],
"materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],
"textures":[{"source":0}],"images":[{"bufferView":3,"mimeType":"image/png"}],
"buffers":[{"byteLength":@BUFFER@}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":48},
{"buffer":0,"byteOffset":48,"byteLength":32},
{"buffer":0,"byteOffset":80,"byteLength":12},
{"buffer":0,"byteOffset":92,"byteLength":@PNG@}],
"accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3",
"min":[-1,-1,0],"max":[1,1,0]},
{"bufferView":1,"componentType":5126,"count":4,"type":"VEC2"},
{"bufferView":2,"componentType":5123,"count":6,"type":"SCALAR"}]})");
    json.replace("@BUFFER@", QByteArray::number(bufferSize));
    json.replace("@PNG@", QByteArray::number(png.size()));
    while (json.size() % 4) {
        json.append(' ');
    }
    QByteArray glb;
    QDataStream file(&glb, QIODevice::WriteOnly);
    file.setByteOrder(QDataStream::LittleEndian);
    file << quint32(0x46546C67) << quint32(2)
         << quint32(12 + 8 + json.size() + 8 + binary.size())
         << quint32(json.size()) << quint32(0x4E4F534A);
    file.writeRawData(json.constData(), json.size());
    file << quint32(binary.size()) << quint32(0x004E4942);
    file.writeRawData(binary.constData(), binary.size());

    const QImage preview = render_model_preview(glb);
    ASSERT_FALSE(preview.isNull());
    qint64 redY = 0, blueY = 0;
    int redCount = 0, blueCount = 0;
    for (int y = 0; y < preview.height(); ++y) {
        for (int x = 0; x < preview.width(); ++x) {
            const QColor color = preview.pixelColor(x, y);
            if (color == Qt::red) {
                redY += y;
                ++redCount;
            } else if (color == Qt::blue) {
                blueY += y;
                ++blueCount;
            }
        }
    }
    ASSERT_GT(redCount, 100);
    ASSERT_GT(blueCount, 100);
    EXPECT_LT(double(redY) / redCount, double(blueY) / blueCount);

    QImage stale(400, 400, QImage::Format_RGB32);
    stale.fill(Qt::magenta);
    QVariantMap saved;
    saved[QStringLiteral("type")] = QStringLiteral("model");
    saved[QStringLiteral("modelBytes")] = glb;
    saved[QStringLiteral("image")] = QVariant::fromValue(stale);
    saved[QStringLiteral("filename")] = QStringLiteral("old.glb");
    saved[QStringLiteral("data")] = QVariantMap{{QStringLiteral("yaw"), 25},
                                                {QStringLiteral("pitch"), -15},
                                                {QStringLiteral("distance"), 450}};
    saved[QStringLiteral("encodedBytes")] = QByteArrayLiteral("stale preview");
    saved[QStringLiteral("encodedFormat")] = QStringLiteral("png");
    std::unique_ptr<IBaseItem> restored(CanvasScene::create_item_from_data(saved));
    auto* model = dynamic_cast<ModelItem*>(restored.get());
    ASSERT_NE(model, nullptr);
    EXPECT_EQ(model->pixmap().toImage(), preview);
    EXPECT_TRUE(model->encoded_source().isEmpty());
}

TEST(ModelCanvasTest, GltfImportBundlesExternalFilesForProjectSave)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QByteArray geometry;
    QDataStream mesh(&geometry, QIODevice::WriteOnly);
    mesh.setByteOrder(QDataStream::LittleEndian);
    mesh.setFloatingPointPrecision(QDataStream::SinglePrecision);
    for (float coordinate : {-1.0f, -1.0f, 0.0f,
                             1.0f, -1.0f, 0.0f,
                             0.0f, 1.0f, 0.0f}) {
        mesh << coordinate;
    }
    for (quint16 index : {0, 1, 2}) {
        mesh << index;
    }
    ASSERT_EQ(geometry.size(), 42);
    const QString binPath = dir.filePath(QStringLiteral("mesh.bin"));
    QFile binFile(binPath);
    ASSERT_TRUE(binFile.open(QIODevice::WriteOnly));
    ASSERT_EQ(binFile.write(geometry), geometry.size());
    binFile.close();

    QImage texture(2, 2, QImage::Format_RGB32);
    texture.fill(Qt::red);
    const QString texturePath = dir.filePath(QStringLiteral("texture.png"));
    ASSERT_TRUE(texture.save(texturePath));

    const QByteArray json = QByteArrayLiteral(R"({
"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],
"nodes":[{"mesh":0}],"meshes":[{"primitives":[{
"attributes":{"POSITION":0},"indices":1}]}],
"buffers":[{"uri":"mesh.bin","byteLength":42}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},
{"buffer":0,"byteOffset":36,"byteLength":6}],
"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3",
"min":[-1,-1,0],"max":[1,1,0]},
{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],
"images":[{"uri":"texture.png"}]})");
    const QString gltfPath = dir.filePath(QStringLiteral("triangle.gltf"));
    QFile gltfFile(gltfPath);
    ASSERT_TRUE(gltfFile.open(QIODevice::WriteOnly));
    ASSERT_EQ(gltfFile.write(json), json.size());
    gltfFile.close();

    const GltfImportResult imported = import_gltf(gltfPath);
    ASSERT_TRUE(imported.error.isEmpty()) << imported.error.toStdString();
    ASSERT_TRUE(imported.glb.startsWith("glTF"));
    quint32 jsonLength = 0;
    std::memcpy(&jsonLength, imported.glb.constData() + 12, sizeof(jsonLength));
    const QJsonObject packed = QJsonDocument::fromJson(
        imported.glb.mid(20, jsonLength)).object();
    EXPECT_EQ(packed.value(QStringLiteral("buffers")).toArray().size(), 1);
    EXPECT_EQ(packed.value(QStringLiteral("bufferViews")).toArray().size(), 3);
    const QJsonObject image = packed.value(QStringLiteral("images"))
                                  .toArray().first().toObject();
    EXPECT_FALSE(image.contains(QStringLiteral("uri")));
    EXPECT_EQ(image.value(QStringLiteral("mimeType")).toString(),
              QStringLiteral("image/png"));

    QFile textureFile(texturePath);
    ASSERT_TRUE(textureFile.open(QIODevice::ReadOnly));
    const QByteArray png = textureFile.readAll();
    textureFile.close();
    QByteArray embeddedJson = json;
    embeddedJson.replace("mesh.bin",
                         "data:application/octet-stream;base64," + geometry.toBase64());
    embeddedJson.replace("texture.png", "data:image/png;base64," + png.toBase64());
    const QString embeddedPath = dir.filePath(QStringLiteral("embedded.gltf"));
    QFile embeddedFile(embeddedPath);
    ASSERT_TRUE(embeddedFile.open(QIODevice::WriteOnly));
    ASSERT_EQ(embeddedFile.write(embeddedJson), embeddedJson.size());
    embeddedFile.close();
    const GltfImportResult embedded = import_gltf(embeddedPath);
    ASSERT_TRUE(embedded.error.isEmpty()) << embedded.error.toStdString();
    EXPECT_EQ(embedded.glb, imported.glb);

    MainWindow window;
    window.show();
    CanvasView* canvas = window.tab_pane().current_widget();
    ASSERT_NE(canvas, nullptr);
    canvas->do_insert_images({QUrl::fromLocalFile(gltfPath)}, QPoint(160, 160));
    const auto models = canvas->scene()->items_by_type("model");
    ASSERT_EQ(models.size(), 1);
    auto* model = dynamic_cast<ModelItem*>(models.first());
    ASSERT_NE(model, nullptr);
    EXPECT_EQ(model->model_bytes(), imported.glb);

    const QString projectPath = dir.filePath(QStringLiteral("model.fml"));
    const FmlResult saved = FmlArchive::save(canvas->scene(), {}, projectPath);
    ASSERT_TRUE(saved.error.isEmpty()) << saved.error.toStdString();
    ASSERT_TRUE(QFile::remove(texturePath));
    EXPECT_FALSE(import_gltf(gltfPath).error.isEmpty());
    ASSERT_TRUE(QFile::remove(gltfPath));
    ASSERT_TRUE(QFile::remove(binPath));

    QUndoStack undo;
    uint64_t zCounter = 0;
    CanvasScene restored(window, zCounter, &undo);
    const FmlResult loaded = FmlArchive::load(projectPath, &restored);
    ASSERT_TRUE(loaded.error.isEmpty()) << loaded.error.toStdString();
    restored.add_queued_items();
    const auto restoredModels = restored.items_by_type("model");
    ASSERT_EQ(restoredModels.size(), 1);
    const auto* restoredModel = dynamic_cast<ModelItem*>(restoredModels.first());
    ASSERT_NE(restoredModel, nullptr);
    EXPECT_EQ(restoredModel->model_bytes(), imported.glb);
}

TEST(ModelCanvasTest, ObjImportBundlesMaterialAndTextureForProjectSave)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ASSERT_TRUE(QDir().mkpath(dir.filePath(QStringLiteral("textures"))));
    const QString texturePath = dir.filePath(QStringLiteral("textures/color.png"));
    QImage texture(2, 2, QImage::Format_RGB32);
    texture.setPixelColor(0, 0, Qt::red);
    texture.setPixelColor(1, 0, Qt::green);
    texture.setPixelColor(0, 1, Qt::blue);
    texture.setPixelColor(1, 1, Qt::yellow);
    ASSERT_TRUE(texture.save(texturePath));
    const QString mtlPath = dir.filePath(QStringLiteral("square.mtl"));
    QFile mtl(mtlPath);
    ASSERT_TRUE(mtl.open(QIODevice::WriteOnly));
    ASSERT_GT(mtl.write("newmtl painted\nKd 1 1 1\n"
                        "map_Kd textures/color.png\n"), 0);
    mtl.close();
    const QString objPath = dir.filePath(QStringLiteral("square.obj"));
    QFile obj(objPath);
    ASSERT_TRUE(obj.open(QIODevice::WriteOnly));
    ASSERT_GT(obj.write("mtllib square.mtl\n"
                        "v -1 -1 0\nv 1 -1 0\nv 1 1 0\nv -1 1 0\n"
                        "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
                        "vn 0 0 1\nusemtl painted\n"
                        "f -4/1/1 -3/2/1 -2/3/1 -1/4/1\n"), 0);
    obj.close();

    const GltfImportResult imported = import_obj(objPath);
    ASSERT_TRUE(imported.error.isEmpty()) << imported.error.toStdString();
    ASSERT_TRUE(imported.glb.startsWith("glTF"));
    quint32 jsonLength = 0;
    std::memcpy(&jsonLength, imported.glb.constData() + 12, sizeof(jsonLength));
    const QJsonObject json = QJsonDocument::fromJson(
        imported.glb.mid(20, jsonLength)).object();
    ASSERT_EQ(json.value(QStringLiteral("meshes")).toArray().size(), 1);
    const QJsonArray primitives = json.value(QStringLiteral("meshes"))
                                      .toArray().first().toObject()
                                      .value(QStringLiteral("primitives")).toArray();
    ASSERT_EQ(primitives.size(), 1);
    const QJsonArray accessors = json.value(QStringLiteral("accessors")).toArray();
    const int indices = primitives.first().toObject()
                            .value(QStringLiteral("indices")).toInt();
    EXPECT_EQ(accessors.at(indices).toObject()
                  .value(QStringLiteral("count")).toInt(), 6);
    EXPECT_EQ(json.value(QStringLiteral("images")).toArray().size(), 1);
    const QImage preview = render_model_preview(imported.glb);
    ASSERT_FALSE(preview.isNull());
    int coloredPixels = 0;
    for (int y = 0; y < preview.height(); ++y) {
        for (int x = 0; x < preview.width(); ++x) {
            const QColor color = preview.pixelColor(x, y);
            if (color == Qt::red || color == Qt::green
                || color == Qt::blue || color == Qt::yellow) {
                ++coloredPixels;
            }
        }
    }
    EXPECT_GT(coloredPixels, 10000);

    MainWindow window;
    window.show();
    CanvasView* canvas = window.tab_pane().current_widget();
    ASSERT_NE(canvas, nullptr);
    canvas->do_insert_images({QUrl::fromLocalFile(objPath)}, QPoint(160, 160));
    const auto models = canvas->scene()->items_by_type("model");
    ASSERT_EQ(models.size(), 1);
    auto* model = dynamic_cast<ModelItem*>(models.first());
    ASSERT_NE(model, nullptr);
    EXPECT_EQ(model->model_bytes(), imported.glb);

    const QString projectPath = dir.filePath(QStringLiteral("object.fml"));
    const FmlResult saved = FmlArchive::save(canvas->scene(), {}, projectPath);
    ASSERT_TRUE(saved.error.isEmpty()) << saved.error.toStdString();
    ASSERT_TRUE(QFile::remove(texturePath));
    EXPECT_FALSE(import_obj(objPath).error.isEmpty());
    ASSERT_TRUE(QFile::remove(mtlPath));
    ASSERT_TRUE(QFile::remove(objPath));
    QUndoStack undo;
    uint64_t zCounter = 0;
    CanvasScene restored(window, zCounter, &undo);
    const FmlResult loaded = FmlArchive::load(projectPath, &restored);
    ASSERT_TRUE(loaded.error.isEmpty()) << loaded.error.toStdString();
    restored.add_queued_items();
    const auto restoredModels = restored.items_by_type("model");
    ASSERT_EQ(restoredModels.size(), 1);
    const auto* restoredModel = dynamic_cast<ModelItem*>(restoredModels.first());
    ASSERT_NE(restoredModel, nullptr);
    EXPECT_EQ(restoredModel->model_bytes(), imported.glb);

    const QString plainPath = dir.filePath(QStringLiteral("plain.obj"));
    QFile plain(plainPath);
    ASSERT_TRUE(plain.open(QIODevice::WriteOnly));
    ASSERT_GT(plain.write("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"), 0);
    plain.close();
    const GltfImportResult untextured = import_obj(plainPath);
    ASSERT_TRUE(untextured.error.isEmpty()) << untextured.error.toStdString();
    const QImage plainPreview = render_model_preview(untextured.glb);
    ASSERT_FALSE(plainPreview.isNull());
    int whitePixels = 0;
    for (int y = 0; y < plainPreview.height(); ++y) {
        for (int x = 0; x < plainPreview.width(); ++x) {
            whitePixels += plainPreview.pixelColor(x, y) == Qt::white;
        }
    }
    EXPECT_GT(whitePixels, 100);
}

TEST(ModelCanvasTest, ModelImportAndUnlock)
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

    const QString sourceFile = qEnvironmentVariable("FAMILIAR_TEST_GLB",
                                                    filename);
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

    EventLoopHeartbeat heartbeat;
    QTest::mouseClick(button, Qt::LeftButton);
    EXPECT_TRUE(button->isChecked());
    EXPECT_TRUE(editViewer->isVisible());
    EXPECT_TRUE(window.isVisible());
    EXPECT_TRUE(editViewer->rootObject()->property("error").toString().isEmpty());
    EXPECT_FALSE(editViewer->rootObject()->property("previewSource").toUrl().isEmpty());
    // Lock and unlock again while conversion is still running. The first
    // process should finish for the same canvas item without opening a
    // second copy or reviving the closed viewer.
    QTest::qWait(50);
    QTest::mouseClick(button, Qt::LeftButton);
    EXPECT_FALSE(button->isChecked());
    QTest::mouseClick(button, Qt::LeftButton);
    EXPECT_TRUE(button->isChecked());
    QTRY_VERIFY_WITH_TIMEOUT(editViewer->rootObject()->property("loaded").toBool(),
                             60000);
    QTest::qWait(100);
    EXPECT_LT(heartbeat.longest_gap(), 4500);

    QTest::mouseClick(button, Qt::LeftButton);
    EXPECT_FALSE(button->isChecked());
    EXPECT_FALSE(editViewer->isVisible());
    EXPECT_TRUE(model->isSelected());
}

TEST(ModelCanvasTest, SavedBoardUnlocksModel)
{
    const QString sourceFile = qEnvironmentVariable("FAMILIAR_TEST_FML");
    if (sourceFile.isEmpty()) {
        GTEST_SKIP() << "Set FAMILIAR_TEST_FML to test a saved board";
    }
    MainWindow window;
    window.show();
    CanvasView* canvas = window.tab_pane().current_widget();
    ASSERT_NE(canvas, nullptr);
    const FmlResult loaded = FmlArchive::load(sourceFile, canvas->scene());
    ASSERT_TRUE(loaded.error.isEmpty()) << loaded.error.toStdString();
    canvas->scene()->add_queued_items();
    const auto models = canvas->scene()->items_by_type("model");
    if (models.isEmpty()) {
        GTEST_SKIP() << "The selected board has no saved 3D model";
    }
    auto* model = dynamic_cast<ModelItem*>(models.first());
    ASSERT_NE(model, nullptr);
    canvas->centerOn(model);
    model->setSelected(true);
    auto* toolbar = canvas->findChild<ModelToolbar*>();
    ASSERT_NE(toolbar, nullptr);
    auto* button = toolbar->findChild<QToolButton*>();
    ASSERT_NE(button, nullptr);
    QTRY_VERIFY(toolbar->isVisible());
    EventLoopHeartbeat heartbeat;
    QTest::mouseClick(button, Qt::LeftButton);
    auto* viewer = canvas->findChild<QQuickWidget*>();
    ASSERT_NE(viewer, nullptr);
    ASSERT_TRUE(viewer->isVisible());
    QTRY_VERIFY_WITH_TIMEOUT(viewer->rootObject()->property("loaded").toBool(),
                             60000);
    QTest::qWait(100);
    EXPECT_LT(heartbeat.longest_gap(), 4500);
    QTest::mouseClick(button, Qt::LeftButton);
    EXPECT_TRUE(window.isVisible());
}
