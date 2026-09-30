#include "widgets/controls/keyboard_shortcuts_page.h"

#include "core/settingshandler.h"

#include <gtest/gtest.h>

// getActions() (the app-wide Action registry, actions/actions.h) is a
// function-local static lazily built on first call - whether it's
// already populated by the time this test runs depends on test link/
// registration order (tests/actions/action_registry_test.cpp's
// GlobalActionRegistryTest also calls it). Either way KeyboardShortcutsPage's
// "Controls" section alone (KeyboardSettings::mouseActions()/
// mousewheelActions(), static data, no MainWindow needed) is enough to
// exercise applySearchFilter()'s own OR-across-both-sections logic -
// "zoom" matches there regardless of what the "Actions" section holds.

TEST(KeyboardShortcutsPageTest, SearchFilterMatchesAgainstControlsSection)
{
    SettingsHandler::get_instance()->remove_json_group(QStringLiteral("Controls"));
    KeyboardShortcutsPage page;

    EXPECT_TRUE(page.apply_search_filter(QStringLiteral("zoom")));
    EXPECT_FALSE(
        page.apply_search_filter(QStringLiteral("this-will-never-match")));
    // Empty text clears the filter - everything visible again.
    EXPECT_TRUE(page.apply_search_filter(QString()));
}
