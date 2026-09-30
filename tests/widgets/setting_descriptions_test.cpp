#include "widgets/setting_descriptions.h"

#include <gtest/gtest.h>

using familiar::setting_descriptions::for_binding_target_id;
using familiar::setting_descriptions::for_settings_key;

namespace {
// Matches setting_descriptions.cpp's own placeholder() literal - no
// public accessor for it (deliberately internal, see its own comment),
// so the known text is duplicated here rather than reached for.
const QString kPlaceholder = QStringLiteral("Description coming soon.");
} // namespace

TEST(SettingDescriptionsTest, KnownSettingsKeyReturnsItsOwnText)
{
    const QString text = for_settings_key(QStringLiteral("Items/arrange_gap"));
    EXPECT_FALSE(text.isEmpty());
    EXPECT_NE(text, kPlaceholder);
}

TEST(SettingDescriptionsTest, UnknownSettingsKeyFallsBackToPlaceholder)
{
    EXPECT_EQ(for_settings_key(QStringLiteral("Nonexistent/key")), kPlaceholder);
}

TEST(SettingDescriptionsTest, KnownBindingTargetIdReturnsItsOwnText)
{
    const QString text = for_binding_target_id(QStringLiteral("open"));
    EXPECT_FALSE(text.isEmpty());
    EXPECT_NE(text, kPlaceholder);
}

TEST(SettingDescriptionsTest, UnknownBindingTargetIdFallsBackToPlaceholder)
{
    EXPECT_EQ(for_binding_target_id(QStringLiteral("nonexistent_id")),
             kPlaceholder);
}
