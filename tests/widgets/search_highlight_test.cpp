#include "widgets/controls/search_highlight.h"

#include <gtest/gtest.h>

TEST(HighlightSearchMatchTest, EmptyQueryReturnsPlainEscapedText)
{
    EXPECT_EQ(highlightSearchMatch(QStringLiteral("A & B"), QString()),
             QStringLiteral("A &amp; B"));
}

TEST(HighlightSearchMatchTest, NoMatchReturnsPlainEscapedText)
{
    EXPECT_EQ(highlightSearchMatch(QStringLiteral("Undo"),
                                  QStringLiteral("zzz")),
             QStringLiteral("Undo"));
}

TEST(HighlightSearchMatchTest, MatchIsBoldedCaseInsensitively)
{
    EXPECT_EQ(highlightSearchMatch(QStringLiteral("Undo History Size"),
                                  QStringLiteral("history")),
             QStringLiteral("Undo <b>History</b> Size"));
}

TEST(HighlightSearchMatchTest, PreservesOriginalCasingOfMatchedSubstring)
{
    // Query casing ("HIST") is only used to FIND the match - the bolded
    // substring itself comes from the original text ("Hist"), not the
    // query.
    EXPECT_EQ(highlightSearchMatch(QStringLiteral("Undo History"),
                                  QStringLiteral("HIST")),
             QStringLiteral("Undo <b>Hist</b>ory"));
}

TEST(HighlightSearchMatchTest, EscapesHtmlSpecialCharactersOutsideTheMatch)
{
    EXPECT_EQ(highlightSearchMatch(QStringLiteral("A & <B> match C"),
                                  QStringLiteral("match")),
             QStringLiteral("A &amp; &lt;B&gt; <b>match</b> C"));
}
