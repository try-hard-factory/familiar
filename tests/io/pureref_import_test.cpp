#include "pureref_import.h"

#include <gtest/gtest.h>

#include <QBuffer>
#include <QCryptographicHash>
#include <QDataStream>
#include <QFile>
#include <QImage>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTransform>
#include <QUuid>
#include <QtEndian>

namespace {

QByteArray encoded_transform(const QTransform& t)
{
    QByteArray bytes = QByteArray::fromHex("0000005000");
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::Append)) {
        return {};
    }
    QDataStream stream(&buffer);
    stream.setByteOrder(QDataStream::BigEndian);
    stream.setFloatingPointPrecision(QDataStream::DoublePrecision);
    stream << t.m11() << t.m12() << t.m13() << t.m21() << t.m22()
           << t.m23() << t.m31() << t.m32() << t.m33();
    return bytes;
}

bool write_fixture(const QString& path, const QString& dbPath)
{
    const QString connection = QUuid::createUuid().toString(QUuid::WithoutBraces);
    bool success = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                     connection);
        db.setDatabaseName(dbPath);
        if (db.open()) {
            QSqlQuery q(db);
            success = q.exec(QStringLiteral(
                          "CREATE TABLE items(id INTEGER PRIMARY KEY,parent INTEGER,"
                          "z REAL,opacity REAL,locked INTEGER,name TEXT,transform TEXT)"))
                      && q.exec(QStringLiteral(
                          "CREATE TABLE images(id INTEGER PRIMARY KEY,data BLOB,"
                          "format TEXT,source TEXT)"))
                      && q.exec(QStringLiteral(
                          "CREATE TABLE items_images(id INTEGER PRIMARY KEY,"
                          "image INTEGER,image_transform TEXT,image_bounds TEXT)"))
                      && q.exec(QStringLiteral(
                          "CREATE TABLE items_notes(id INTEGER PRIMARY KEY,text TEXT,"
                          "background_color TEXT)"))
                      && q.exec(QStringLiteral(
                          "CREATE TABLE items_groups(id INTEGER PRIMARY KEY,"
                          "background_color TEXT)"))
                      && q.exec(QStringLiteral(
                          "CREATE TABLE items_drawings(id INTEGER PRIMARY KEY)"));
            if (success) {
                QImage image(2, 2, QImage::Format_RGB32);
                image.fill(Qt::red);
                QByteArray png;
                QBuffer pngBuffer(&png);
                pngBuffer.open(QIODevice::WriteOnly);
                success = image.save(&pngBuffer, "PNG");
                if (success) {
                    q.prepare(QStringLiteral("INSERT INTO images VALUES(1,?, 'png', 'red.png')"));
                    q.addBindValue(png);
                    success = q.exec();
                }
                const auto addItem = [&q](int id, int parent, int z,
                                          const QTransform& transform) {
                    q.prepare(QStringLiteral(
                        "INSERT INTO items VALUES(?,?,?,1,0,'test',?)"));
                    q.addBindValue(id);
                    q.addBindValue(parent);
                    q.addBindValue(z);
                    q.addBindValue(QString::fromLatin1(encoded_transform(transform)));
                    return q.exec();
                };
                success = success
                          && addItem(1, -1, 1,
                                     QTransform::fromTranslate(10, 20))
                          && addItem(2, 1, 2,
                                     QTransform::fromScale(2, 2)
                                         * QTransform::fromTranslate(100, 200))
                          && addItem(3, -1, 3,
                                     QTransform::fromTranslate(50, 60));
                if (success) {
                    success = q.exec(QStringLiteral("INSERT INTO items_groups VALUES(1,NULL)"));
                }
                if (success) {
                    q.prepare(QStringLiteral("INSERT INTO items_images VALUES(2,1,?,NULL)"));
                    q.addBindValue(QString::fromLatin1(encoded_transform(
                        QTransform::fromTranslate(-1, -1))));
                    success = q.exec();
                }
                if (success) {
                    success = q.exec(QStringLiteral(
                        "INSERT INTO items_notes VALUES(3,'<p>Hello</p>', '#d9ffffff')"));
                }
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connection);
    if (!success) {
        return false;
    }
    QFile dbFile(dbPath);
    if (!dbFile.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray database = dbFile.readAll();
    constexpr int split = 108;
    if (database.size() <= split) {
        return false;
    }
    QByteArray header(split, '\0');
    qToBigEndian<quint32>(6, reinterpret_cast<uchar*>(header.data()));
    header.replace(4, 8, QByteArray::fromHex("0032002e00310000"));
    qToBigEndian<quint64>(static_cast<quint64>(database.size()),
                         reinterpret_cast<uchar*>(header.data() + 14));
    const QByteArray rotated = database.mid(split) + database.left(split);
    const QByteArray digest = QCryptographicHash::hash(
        header.mid(104) + rotated, QCryptographicHash::Md5).toHex();
    for (int i = 0; i < digest.size(); ++i) {
        header[40 + i * 2 + 1] = digest[i];
    }
    QFile output(path);
    return output.open(QIODevice::WriteOnly)
           && output.write(header) == header.size()
           && output.write(rotated) == rotated.size();
}

TEST(PureRefImportTest, ImportsImageNoteAndGroupWithSceneTransforms)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("test.pur"));
    ASSERT_TRUE(write_fixture(path, dir.filePath(QStringLiteral("scene.sqlite"))));

    const auto result = familiar::pureref::read_project(path);
    ASSERT_TRUE(result.error.isEmpty()) << result.error.toStdString();
    ASSERT_EQ(result.items.size(), 3);
    EXPECT_TRUE(result.warnings.isEmpty());
    const QVariantMap image = result.items.at(1);
    EXPECT_EQ(image.value(QStringLiteral("type")).toString(), QStringLiteral("pixmap"));
    EXPECT_EQ(image.value(QStringLiteral("filename")).toString(), QStringLiteral("red.png"));
    EXPECT_EQ(image.value(QStringLiteral("image")).value<QImage>().size(), QSize(2, 2));
    EXPECT_NEAR(image.value(QStringLiteral("x")).toDouble(), 108, 1e-6);
    EXPECT_NEAR(image.value(QStringLiteral("y")).toDouble(), 218, 1e-6);
    EXPECT_NEAR(image.value(QStringLiteral("scale")).toDouble(), 2, 1e-6);
    const QVariantList members = result.items.at(0)
                                     .value(QStringLiteral("data"))
                                     .toMap()
                                     .value(QStringLiteral("child_ids"))
                                     .toList();
    ASSERT_EQ(members.size(), 1);
    EXPECT_EQ(members.first().toString(),
              image.value(QStringLiteral("id")).toUuid().toString(
                  QUuid::WithoutBraces));
    EXPECT_EQ(result.items.at(2).value(QStringLiteral("type")).toString(),
              QStringLiteral("text"));
}

TEST(PureRefImportTest, RejectsDamagedPayloadBeforeReadingDatabase)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("test.pur"));
    ASSERT_TRUE(write_fixture(path, dir.filePath(QStringLiteral("scene.sqlite"))));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::ReadWrite));
    ASSERT_TRUE(file.seek(file.size() - 1));
    ASSERT_EQ(file.write("x", 1), 1);
    file.close();
    const auto result = familiar::pureref::read_project(path);
    EXPECT_FALSE(result.error.isEmpty());
    EXPECT_TRUE(result.items.isEmpty());
}

} // namespace
