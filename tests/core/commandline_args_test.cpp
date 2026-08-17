#include "core/settings.h"

#include <gtest/gtest.h>

TEST(CommandlineArgsTest, FileOptionOverridesPositionalArgument)
{
    CommandlineArgs& args = CommandlineArgs::instance();

    args.parse({QStringLiteral("familiar"), QStringLiteral("/tmp/scene.fml")});
    EXPECT_EQ(args.filename(), QStringLiteral("/tmp/scene.fml"));

    args.parse({QStringLiteral("familiar"),
               QStringLiteral("/tmp/scene.fml"),
               QStringLiteral("--file"),
               QStringLiteral("/tmp/other.fml")});
    EXPECT_EQ(args.filename(), QStringLiteral("/tmp/other.fml"));
}
