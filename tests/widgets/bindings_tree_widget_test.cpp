#include "widgets/controls/bindings_tree_widget.h"

#include "core/settingshandler.h"
#include "widgets/controls/binding_target.h"

#include <gtest/gtest.h>

#include <QLayout>
#include <QToolButton>

// One container widget per target is added directly to the tree's own
// QVBoxLayout, in target order (BindingsTreeWidget's constructor) - no
// public accessor for a specific row, so layout()->itemAt(i)->widget()
// is used instead. Visibility is checked via isHidden() (the widget's
// own explicit hidden flag), not isVisible() (effective/ancestor-chain
// visibility, which would read false regardless of setVisible(true)
// here since the tree itself is never shown() - no need to, none of
// this exercises real paint/click behavior).

TEST(BindingsTreeWidgetTest, EmptyFilterShowsEveryRow)
{
    SettingsHandler::getInstance()->removeJsonGroup(QStringLiteral("Controls"));
    const MouseConfig& zoomCfg = KeyboardSettings::mouseActions()[0]; // "Zoom"
    const MouseConfig& panCfg = KeyboardSettings::mouseActions()[1]; // "Pan"
    MouseConfigBindingTarget zoom(&zoomCfg, BindingTargetKind::MouseControl);
    MouseConfigBindingTarget pan(&panCfg, BindingTargetKind::MouseControl);

    BindingsTreeWidget tree({&zoom, &pan});

    EXPECT_TRUE(tree.applySearchFilter(QString()));
    ASSERT_EQ(tree.layout()->count(), 2);
    EXPECT_FALSE(tree.layout()->itemAt(0)->widget()->isHidden());
    EXPECT_FALSE(tree.layout()->itemAt(1)->widget()->isHidden());
}

TEST(BindingsTreeWidgetTest, FilterHidesNonMatchingRows)
{
    SettingsHandler::getInstance()->removeJsonGroup(QStringLiteral("Controls"));
    const MouseConfig& zoomCfg = KeyboardSettings::mouseActions()[0]; // "Zoom"
    const MouseConfig& panCfg = KeyboardSettings::mouseActions()[1]; // "Pan"
    MouseConfigBindingTarget zoom(&zoomCfg, BindingTargetKind::MouseControl);
    MouseConfigBindingTarget pan(&panCfg, BindingTargetKind::MouseControl);

    BindingsTreeWidget tree({&zoom, &pan});

    EXPECT_TRUE(tree.applySearchFilter(QStringLiteral("zoom")));
    EXPECT_FALSE(tree.layout()->itemAt(0)->widget()->isHidden()); // Zoom matches
    EXPECT_TRUE(tree.layout()->itemAt(1)->widget()->isHidden()); // Pan doesn't
}

TEST(BindingsTreeWidgetTest, NoMatchReturnsFalseAndHidesEverything)
{
    SettingsHandler::getInstance()->removeJsonGroup(QStringLiteral("Controls"));
    const MouseConfig& zoomCfg = KeyboardSettings::mouseActions()[0];
    MouseConfigBindingTarget zoom(&zoomCfg, BindingTargetKind::MouseControl);

    BindingsTreeWidget tree({&zoom});

    EXPECT_FALSE(tree.applySearchFilter(QStringLiteral("nonexistent")));
    EXPECT_TRUE(tree.layout()->itemAt(0)->widget()->isHidden());
}

TEST(CollapsibleSectionTest, SetExpandedTogglesContentVisibilityAndArrow)
{
    auto* content = new QWidget; // ownership transferred to CollapsibleSection
    CollapsibleSection section(QStringLiteral("Test Section"), content);

    QToolButton* header = section.findChild<QToolButton*>();
    ASSERT_NE(header, nullptr);
    EXPECT_TRUE(header->isChecked());
    EXPECT_EQ(header->arrowType(), Qt::DownArrow);
    EXPECT_FALSE(content->isHidden());

    section.setExpanded(false);
    EXPECT_FALSE(header->isChecked());
    EXPECT_EQ(header->arrowType(), Qt::RightArrow);
    EXPECT_TRUE(content->isHidden());

    section.setExpanded(true);
    EXPECT_TRUE(header->isChecked());
    EXPECT_EQ(header->arrowType(), Qt::DownArrow);
    EXPECT_FALSE(content->isHidden());
}
