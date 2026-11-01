#include "fml_archive.h"

#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

// A cycle in a .fml's group graph used to hang the application on load:
// GroupItem::fit_to_contain_children() and GroupItem::paint() climb the
// ancestor chain with an unguarded `for (a = this; a; a = owner(a))`, so
// "A contains B, B contains A" never terminates. The loader validated ids
// for duplicates only. These tests feed sanitize_item_graph() exactly the
// manifest shape a crafted file would carry.

namespace {

QJsonObject group(const QString& id, const QStringList& childIds)
{
    QJsonArray children;
    for (const QString& child : childIds) {
        children.append(child);
    }
    QJsonObject data;
    data[QStringLiteral("child_ids")] = children;
    QJsonObject item;
    item[QStringLiteral("id")] = id;
    item[QStringLiteral("type")] = QStringLiteral("group");
    item[QStringLiteral("data")] = data;
    return item;
}

QJsonObject picture(const QString& id, const QString& attachedTo = QString())
{
    QJsonObject data;
    if (!attachedTo.isEmpty()) {
        data[QStringLiteral("attached_to")] = attachedTo;
    }
    QJsonObject item;
    item[QStringLiteral("id")] = id;
    item[QStringLiteral("type")] = QStringLiteral("pixmap");
    item[QStringLiteral("data")] = data;
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

QStringList child_ids_of(const QJsonObject& root, const QString& id)
{
    QStringList out;
    for (const QJsonValue& v : root.value(QStringLiteral("items")).toArray()) {
        const QJsonObject item = v.toObject();
        if (item.value(QStringLiteral("id")).toString() != id) {
            continue;
        }
        for (const QJsonValue& child : item.value(QStringLiteral("data"))
                                           .toObject()
                                           .value(QStringLiteral("child_ids"))
                                           .toArray()) {
            out.append(child.toString());
        }
    }
    return out;
}

bool has_attachment(const QJsonObject& root, const QString& id)
{
    for (const QJsonValue& v : root.value(QStringLiteral("items")).toArray()) {
        const QJsonObject item = v.toObject();
        if (item.value(QStringLiteral("id")).toString() == id) {
            return item.value(QStringLiteral("data"))
                .toObject()
                .contains(QStringLiteral("attached_to"));
        }
    }
    return false;
}

} // namespace

// The baseline: a valid forest must survive untouched and report nothing.
TEST(FmlGraphSanitizeTest, LeavesAValidForestAlone)
{
    QJsonObject root = manifest_of({group("g1", {"g2", "p1"}),
                                    group("g2", {"p2"}),
                                    picture("p1"),
                                    picture("p2", "p1")});

    const QStringList reports = FmlArchive::sanitize_item_graph(root);

    EXPECT_TRUE(reports.isEmpty());
    EXPECT_EQ(child_ids_of(root, "g1"), QStringList({"g2", "p1"}));
    EXPECT_EQ(child_ids_of(root, "g2"), QStringList({"p2"}));
    EXPECT_TRUE(has_attachment(root, "p2"));
}

TEST(FmlGraphSanitizeTest, DropsGroupContainingItself)
{
    QJsonObject root = manifest_of({group("g1", {"g1", "p1"}), picture("p1")});

    const QStringList reports = FmlArchive::sanitize_item_graph(root);

    EXPECT_EQ(reports.size(), 1);
    EXPECT_EQ(child_ids_of(root, "g1"), QStringList({"p1"}));
}

// The reported repro: A contains B while B contains A.
TEST(FmlGraphSanitizeTest, BreaksTwoGroupCycle)
{
    QJsonObject root = manifest_of({group("gA", {"gB"}), group("gB", {"gA"})});

    const QStringList reports = FmlArchive::sanitize_item_graph(root);

    ASSERT_EQ(reports.size(), 1);
    // Exactly one of the two edges survives - whichever came first - so
    // the graph is a chain, which the ancestor walks terminate on.
    const int remaining = int(child_ids_of(root, "gA").size()
                              + child_ids_of(root, "gB").size());
    EXPECT_EQ(remaining, 1);
}

TEST(FmlGraphSanitizeTest, BreaksLongerCycle)
{
    QJsonObject root = manifest_of({group("gA", {"gB"}),
                                    group("gB", {"gC"}),
                                    group("gC", {"gA"})});

    const QStringList reports = FmlArchive::sanitize_item_graph(root);

    ASSERT_EQ(reports.size(), 1);
    const int remaining = int(child_ids_of(root, "gA").size()
                              + child_ids_of(root, "gB").size()
                              + child_ids_of(root, "gC").size());
    EXPECT_EQ(remaining, 2);
}

// Two groups both claiming the same member: find_owning_group() returns
// whichever it happens to hit first, so the second claim is corruption.
TEST(FmlGraphSanitizeTest, DropsDoubleMembership)
{
    QJsonObject root = manifest_of(
        {group("g1", {"p1"}), group("g2", {"p1"}), picture("p1")});

    const QStringList reports = FmlArchive::sanitize_item_graph(root);

    EXPECT_EQ(reports.size(), 1);
    EXPECT_EQ(child_ids_of(root, "g1"), QStringList({"p1"}));
    EXPECT_TRUE(child_ids_of(root, "g2").isEmpty());
}

TEST(FmlGraphSanitizeTest, DropsSelfAttachment)
{
    QJsonObject root = manifest_of({picture("p1", "p1")});

    const QStringList reports = FmlArchive::sanitize_item_graph(root);

    EXPECT_EQ(reports.size(), 1);
    EXPECT_FALSE(has_attachment(root, "p1"));
}

TEST(FmlGraphSanitizeTest, BreaksAttachmentCycle)
{
    QJsonObject root = manifest_of({picture("p1", "p2"), picture("p2", "p1")});

    const QStringList reports = FmlArchive::sanitize_item_graph(root);

    ASSERT_EQ(reports.size(), 1);
    EXPECT_TRUE(has_attachment(root, "p1"));
    EXPECT_FALSE(has_attachment(root, "p2"));
}

// An id nothing in the manifest claims cannot resolve and cannot close a
// cycle - resolve_children() already ignores it, so it is left in place
// rather than reported as corruption.
TEST(FmlGraphSanitizeTest, KeepsDanglingChildId)
{
    QJsonObject root = manifest_of({group("g1", {"missing"})});

    const QStringList reports = FmlArchive::sanitize_item_graph(root);

    EXPECT_TRUE(reports.isEmpty());
    EXPECT_EQ(child_ids_of(root, "g1"), QStringList({"missing"}));
}
