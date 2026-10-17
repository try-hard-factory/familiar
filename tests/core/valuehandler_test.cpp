#include "core/valuehandler.h"

#include "core/settingshandler.h"

#include <gtest/gtest.h>

#include <QVariant>

// Only Bool/BoundedInt/OpacityList/ColorList are covered here - the only
// ValueHandler subclasses actually instantiated anywhere in this app
// (via the OPTION(...) table in core/settingshandler.cpp). Color and
// KeySequence (also declared in core/valuehandler.h) have no call site
// at all - dead code, presumably left over from this file's Flameshot
// origin (see Color::process()'s own "flameshot" comment) - not tested
// here since there's no real usage to protect and their exact calling
// convention (what representation()'s `val` is actually populated with)
// can't be confirmed against any real caller.

// ─── ValueHandler::value() base orchestration ──────────────────────────
// Exercised via BoundedInt, whose process() is the untouched base-class
// passthrough - isolates value()'s own dispatch logic (invalid ->
// fallback, check() fails -> fallback, otherwise process()) from any
// subclass-specific process() behavior.

TEST(ValueHandlerBaseTest, InvalidVariantReturnsFallback)
{
    BoundedInt handler(0, 10, 5);
    EXPECT_EQ(handler.value(QVariant()), QVariant(5));
}

TEST(ValueHandlerBaseTest, FailedCheckReturnsFallback)
{
    BoundedInt handler(0, 10, 5);
    EXPECT_EQ(handler.value(QVariant(999)), QVariant(5));
}

TEST(ValueHandlerBaseTest, PassedCheckReturnsProcessedValue)
{
    BoundedInt handler(0, 10, 5);
    EXPECT_EQ(handler.value(QVariant(7)), QVariant(7));
}

// ─── Bool ────────────────────────────────────────────────────────────

TEST(BoolValueHandlerTest, ChecksLiteralStringsTrueFalseOnly)
{
    Bool handler(true);
    EXPECT_TRUE(handler.check(QStringLiteral("true")));
    EXPECT_TRUE(handler.check(QStringLiteral("false")));
    EXPECT_FALSE(handler.check(QStringLiteral("yes")));
    EXPECT_FALSE(handler.check(QVariant(1)));
}

TEST(BoolValueHandlerTest, FallbackIsConstructorDefault)
{
    EXPECT_EQ(Bool(true).fallback(), QVariant(true));
    EXPECT_EQ(Bool(false).fallback(), QVariant(false));
}

// ─── BoundedInt ──────────────────────────────────────────────────────

TEST(BoundedIntValueHandlerTest, ChecksRangeInclusive)
{
    BoundedInt handler(0, 10, 5);
    EXPECT_TRUE(handler.check(0));
    EXPECT_TRUE(handler.check(10));
    EXPECT_TRUE(handler.check(5));
    EXPECT_FALSE(handler.check(-1));
    EXPECT_FALSE(handler.check(11));
}

TEST(BoundedIntValueHandlerTest, RejectsNonNumeric)
{
    BoundedInt handler(0, 10, 5);
    EXPECT_FALSE(handler.check(QStringLiteral("abc")));
}

TEST(BoundedIntValueHandlerTest, ExpectedDescribesRange)
{
    BoundedInt handler(0, 10, 5);
    EXPECT_EQ(handler.expected(), QStringLiteral("number between 0 and 10"));
}

// ─── ColorList ───────────────────────────────────────────────────────
// The real storage shape for e.g. "dark_color_preset" (settingshandler.cpp)
// - a JSON array of hex strings, index = QMap key. See ColorList::process()'s
// own comment for why this replaced storing the QMap directly.

TEST(ColorListValueHandlerTest, ProcessConvertsIndexedHexStringsToColorMap)
{
    ColorList handler({});
    const QVariantList raw = {QStringLiteral("#ff0000"),
                              QStringLiteral("#00ff00")};

    const auto map
        = handler.process(raw).value<QMap<EPresetsColorIdx, QColor>>();
    EXPECT_EQ(map.value(EPresetsColorIdx::BackgroundColor),
              QColor(QStringLiteral("#ff0000")));
    EXPECT_EQ(map.value(EPresetsColorIdx::CanvasColor),
              QColor(QStringLiteral("#00ff00")));
}

TEST(ColorListValueHandlerTest,
     RepresentationConvertsColorMapToIndexedHexArgbStrings)
{
    QMap<EPresetsColorIdx, QColor> map;
    map[EPresetsColorIdx::BackgroundColor] = QColor(255, 0, 0);
    map[EPresetsColorIdx::CanvasColor] = QColor(0, 255, 0, 128);
    ColorList handler({});

    const QVariantList list
        = handler.representation(QVariant::fromValue(map)).toList();
    ASSERT_EQ(list.size(), 2);
    EXPECT_EQ(list[0].toString(), QColor(255, 0, 0).name(QColor::HexArgb));
    EXPECT_EQ(list[1].toString(), QColor(0, 255, 0, 128).name(QColor::HexArgb));
}

TEST(ColorListValueHandlerTest, FallbackReturnsConstructorDefault)
{
    // The comma inside the QMap's template args would otherwise
    // be read as a THIRD macro argument by EXPECT_EQ - the preprocessor
    // matches (), [], {} but not <>, so it can't tell a template
    // argument list from an actual argument separator. A type alias
    // sidesteps it entirely.
    using ColorMap = QMap<EPresetsColorIdx, QColor>;
    ColorMap def;
    def[EPresetsColorIdx::BackgroundColor] = QColor(Qt::black);
    ColorList handler(def);
    EXPECT_EQ(handler.fallback().value<ColorMap>(), def);
}

TEST(ColorListValueHandlerTest, CheckIsAlwaysTrue)
{
    // check() is a stub here - see its own comment in valuehandler.cpp
    // for why (the QMap shape only round-tripped through the
    // old QSettings backend "by accident"). process() is what actually
    // matters for this type, not check().
    ColorList handler({});
    EXPECT_TRUE(handler.check(QVariant()));
    EXPECT_TRUE(handler.check(QStringLiteral("garbage")));
}

// ─── OpacityList ─────────────────────────────────────────────────────
// Same indexed-storage shape as ColorList, just plain ints (used for
// "master_opacity" - one opacity per color preset).

TEST(OpacityListValueHandlerTest, ProcessConvertsIndexedIntsToMap)
{
    OpacityList handler({});
    const QVariantList raw = {10, 20, 30};

    const auto map = handler.process(raw).value<QMap<EPresets, int>>();
    EXPECT_EQ(map.value(EPresets::DarkPreset), 10);
    EXPECT_EQ(map.value(EPresets::LightPreset), 20);
    EXPECT_EQ(map.value(EPresets::Custom1), 30);
}

TEST(OpacityListValueHandlerTest, RepresentationConvertsMapToIndexedList)
{
    QMap<EPresets, int> map = {{EPresets::DarkPreset, 10},
                               {EPresets::LightPreset, 20}};
    OpacityList handler({});

    const QVariantList list
        = handler.representation(QVariant::fromValue(map)).toList();
    ASSERT_EQ(list.size(), 2);
    EXPECT_EQ(list[0].toInt(), 10);
    EXPECT_EQ(list[1].toInt(), 20);
}

TEST(OpacityListValueHandlerTest, FallbackReturnsConstructorDefault)
{
    // Same EXPECT_EQ-vs-template-comma issue as
    // ColorListValueHandlerTest.FallbackReturnsConstructorDefault above.
    using OpacityMap = QMap<EPresets, int>;
    OpacityMap def = {{EPresets::DarkPreset, 255}};
    OpacityList handler(def);
    EXPECT_EQ(handler.fallback().value<OpacityMap>(), def);
}
