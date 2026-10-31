#include "fml_archive.h"

#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

#include <cmath>

// The transform fields of a .fml item reached QGraphicsItem's own
// setters untouched: CanvasScene::create_item_from_data() calls
// setScale()/setRotation()/setPos() directly, so none of the guards in
// BaseItemMixin apply. A hand-edited or corrupted file could therefore
// plant a scale of 0 (unrecoverable by hand - every later handle-driven
// scale is scale_orig_factor() * factor, which stays 0) or a coordinate
// near the largest double JSON can carry (1.8e308, which overflows to
// inf as soon as it is multiplied by the item's size).
//
// NaN and inf themselves are deliberately not tested: Qt's JSON parser
// rejects `NaN`, `Infinity` and `1e999` as illegal numbers before
// parse_manifest() ever sees the items, so they cannot reach here.

namespace {

QJsonObject item_with(const QString& id, const QVariantMap& fields)
{
    QJsonObject item;
    item[QStringLiteral("id")] = id;
    item[QStringLiteral("type")] = QStringLiteral("pixmap");
    for (auto it = fields.cbegin(); it != fields.cend(); ++it) {
        item[it.key()] = it.value().toDouble();
    }
    return item;
}

QJsonObject manifest_of(const QList<QJsonObject>& items)
{
    QJsonArray array;
    for (const QJsonObject& item : items) {
        array.append(item);
    }
    QJsonObject root;
    root[QStringLiteral("items")] = array;
    return root;
}

double field_of(const QJsonObject& root, const QString& id, const QString& key)
{
    for (const QJsonValue& v : root.value(QStringLiteral("items")).toArray()) {
        const QJsonObject item = v.toObject();
        if (item.value(QStringLiteral("id")).toString() == id) {
            return item.value(key).toDouble();
        }
    }
    return std::nan("");
}

constexpr double kjsonMaxDouble = 1.7976931348623157e308;

} // namespace

TEST(FmlTransformSanitizeTest, AZeroScaleIsResetToOne)
{
    QJsonObject root
        = manifest_of({item_with(QStringLiteral("a"), {{"scale", 0.0}})});

    const QStringList reports = FmlArchive::sanitize_item_transforms(root);

    EXPECT_EQ(reports.size(), 1);
    EXPECT_DOUBLE_EQ(field_of(root, QStringLiteral("a"), "scale"), 1.0);
}

TEST(FmlTransformSanitizeTest, ANegativeScaleIsResetToOne)
{
    QJsonObject root
        = manifest_of({item_with(QStringLiteral("a"), {{"scale", -3.0}})});

    const QStringList reports = FmlArchive::sanitize_item_transforms(root);

    EXPECT_EQ(reports.size(), 1);
    EXPECT_DOUBLE_EQ(field_of(root, QStringLiteral("a"), "scale"), 1.0);
}

TEST(FmlTransformSanitizeTest, AnAbsurdScaleIsClamped)
{
    QJsonObject root = manifest_of(
        {item_with(QStringLiteral("a"), {{"scale", kjsonMaxDouble}})});

    const QStringList reports = FmlArchive::sanitize_item_transforms(root);

    EXPECT_EQ(reports.size(), 1);
    const double scale = field_of(root, QStringLiteral("a"), "scale");
    EXPECT_TRUE(std::isfinite(scale));
    EXPECT_LE(scale, 1e6);
    EXPECT_GT(scale, 0.0);
}

TEST(FmlTransformSanitizeTest, AnOrdinaryScaleIsLeftAlone)
{
    QJsonObject root
        = manifest_of({item_with(QStringLiteral("a"), {{"scale", 0.25}})});

    const QStringList reports = FmlArchive::sanitize_item_transforms(root);

    EXPECT_TRUE(reports.isEmpty());
    EXPECT_DOUBLE_EQ(field_of(root, QStringLiteral("a"), "scale"), 0.25);
}

TEST(FmlTransformSanitizeTest, CoordinatesFarOutsideAnySceneAreClamped)
{
    QJsonObject root = manifest_of({item_with(QStringLiteral("a"),
                                              {{"x", kjsonMaxDouble},
                                               {"y", -kjsonMaxDouble},
                                               {"z", kjsonMaxDouble}})});

    const QStringList reports = FmlArchive::sanitize_item_transforms(root);

    // One per field.
    EXPECT_EQ(reports.size(), 3);
    EXPECT_DOUBLE_EQ(field_of(root, QStringLiteral("a"), "x"), 1e9);
    EXPECT_DOUBLE_EQ(field_of(root, QStringLiteral("a"), "y"), -1e9);
    EXPECT_DOUBLE_EQ(field_of(root, QStringLiteral("a"), "z"), 1e9);
}

TEST(FmlTransformSanitizeTest, OrdinaryCoordinatesAreLeftAlone)
{
    QJsonObject root = manifest_of(
        {item_with(QStringLiteral("a"),
                   {{"x", -1234.5}, {"y", 9876.25}, {"z", 7.0}})});

    const QStringList reports = FmlArchive::sanitize_item_transforms(root);

    EXPECT_TRUE(reports.isEmpty());
    EXPECT_DOUBLE_EQ(field_of(root, QStringLiteral("a"), "x"), -1234.5);
    EXPECT_DOUBLE_EQ(field_of(root, QStringLiteral("a"), "y"), 9876.25);
    EXPECT_DOUBLE_EQ(field_of(root, QStringLiteral("a"), "z"), 7.0);
}

// set_rotation() stores whatever it is handed, so a -90 or a 450 is
// ordinary output from a previous session rather than corruption: the
// value is folded into the range the cursor logic in selector.h assumes,
// but nothing is reported.
TEST(FmlTransformSanitizeTest, RotationIsFoldedIntoOneTurnWithoutReporting)
{
    QJsonObject root = manifest_of({
        item_with(QStringLiteral("neg"), {{"rotation", -90.0}}),
        item_with(QStringLiteral("over"), {{"rotation", 450.0}}),
        item_with(QStringLiteral("huge"), {{"rotation", kjsonMaxDouble}}),
    });

    const QStringList reports = FmlArchive::sanitize_item_transforms(root);

    EXPECT_TRUE(reports.isEmpty());
    EXPECT_DOUBLE_EQ(field_of(root, QStringLiteral("neg"), "rotation"), 270.0);
    EXPECT_DOUBLE_EQ(field_of(root, QStringLiteral("over"), "rotation"), 90.0);

    const double huge = field_of(root, QStringLiteral("huge"), "rotation");
    EXPECT_GE(huge, 0.0);
    EXPECT_LT(huge, 360.0);
}

TEST(FmlTransformSanitizeTest, AnAlreadyNormalizedRotationIsLeftAlone)
{
    QJsonObject root
        = manifest_of({item_with(QStringLiteral("a"), {{"rotation", 33.5}})});

    const QStringList reports = FmlArchive::sanitize_item_transforms(root);

    EXPECT_TRUE(reports.isEmpty());
    EXPECT_DOUBLE_EQ(field_of(root, QStringLiteral("a"), "rotation"), 33.5);
}

// A manifest whose items carry no transform fields at all must keep
// relying on create_item_from_data()'s defaults, not gain explicit ones.
TEST(FmlTransformSanitizeTest, AbsentFieldsAreNotInvented)
{
    QJsonObject item;
    item[QStringLiteral("id")] = QStringLiteral("a");
    item[QStringLiteral("type")] = QStringLiteral("pixmap");
    QJsonObject root = manifest_of({item});

    const QStringList reports = FmlArchive::sanitize_item_transforms(root);

    EXPECT_TRUE(reports.isEmpty());
    const QJsonObject after
        = root.value(QStringLiteral("items")).toArray().at(0).toObject();
    EXPECT_FALSE(after.contains(QStringLiteral("scale")));
    EXPECT_FALSE(after.contains(QStringLiteral("rotation")));
    EXPECT_FALSE(after.contains(QStringLiteral("x")));
}
