#include <gtest/gtest.h>

#include "services/dictionary/GlossaryMatcher.h"

namespace {

using Pairs = GlossaryMatcher::Pairs;

QString u(const char* s) { return QString::fromUtf8(s); }

QStringList keys(const Pairs& pairs) {
    QStringList out;
    for (const auto& p : pairs) out << p.first;
    return out;
}

}

TEST(GlossaryMatcher, ExactTermIsReturnedWithItsTranslation) {
    const Pairs entries{ { u("碳纤维"), u("carbon fiber") } };
    const Pairs hits = GlossaryMatcher::match(entries, u("碳纤维很轻"));
    ASSERT_EQ(hits.size(), 1);
    EXPECT_EQ(hits[0].first, u("碳纤维"));
    EXPECT_EQ(hits[0].second, u("carbon fiber"));
}

TEST(GlossaryMatcher, UnrelatedSentenceHasNoHits) {
    const Pairs entries{ { u("碳纤维"), u("carbon fiber") }, { u("月港"), u("Moon Harbor") } };
    EXPECT_TRUE(GlossaryMatcher::match(entries, u("你好")).isEmpty());
}

TEST(GlossaryMatcher, OneWrongCharacterStillMatchesAtThreshold70) {
    const Pairs entries{ { u("月光港湾"), u("Moonlight Bay") } };
    EXPECT_EQ(keys(GlossaryMatcher::match(entries, u("我们去月光港弯玩"))), QStringList{ u("月光港湾") });
}

TEST(GlossaryMatcher, FuzzyMatchBelowThresholdIsRejected) {
    const Pairs entries{ { u("月光港湾"), u("Moonlight Bay") } };
    EXPECT_TRUE(GlossaryMatcher::match(entries, u("我们去月玩耍"), 10, 90.0).isEmpty());
}

TEST(GlossaryMatcher, SlashSeparatedAliasesPairUpOneToOne) {
    const Pairs entries{ { u("上午/早晨"), u("morning/dawn") } };
    const Pairs hits = GlossaryMatcher::match(entries, u("早晨很安静"));
    ASSERT_EQ(hits.size(), 1);
    EXPECT_EQ(hits[0].first, u("早晨"));
    EXPECT_EQ(hits[0].second, u("dawn"));
}

TEST(GlossaryMatcher, UnevenAliasCountsGiveEveryKeyAllValues) {
    const Pairs entries{ { u("上午/早晨"), u("morning") } };
    const Pairs hits = GlossaryMatcher::match(entries, u("上午好"));
    ASSERT_EQ(hits.size(), 1);
    EXPECT_EQ(hits[0].second, u("morning"));
}

TEST(GlossaryMatcher, BracketAlternativesExpandIntoSeparateKeys) {
    const Pairs entries{ { u("上课（学习/睡觉）"), u("class") } };
    EXPECT_EQ(keys(GlossaryMatcher::match(entries, u("他在上课睡觉"))).size(), 1);
    EXPECT_EQ(keys(GlossaryMatcher::match(entries, u("他在上课学习"))).size(), 1);
}

TEST(GlossaryMatcher, BracketContentIsOptionalInTheSentence) {
    const Pairs entries{ { u("上午（这个）太阳"), u("sun") } };
    EXPECT_EQ(GlossaryMatcher::match(entries, u("上午太阳很大")).size(), 1);
    EXPECT_EQ(GlossaryMatcher::match(entries, u("上午这个太阳很大")).size(), 1);
}

TEST(GlossaryMatcher, FullWidthLettersAreFoldedByNfkc) {
    const Pairs entries{ { u("ＡＢＣ公司"), u("ABC Inc") } };
    EXPECT_EQ(GlossaryMatcher::match(entries, u("ABC公司发布")).size(), 1);
}

TEST(GlossaryMatcher, ExactMatchOutranksAndCoversAShorterTerm) {
    const Pairs entries{ { u("苹果"), u("apple") }, { u("苹果手机"), u("iPhone") } };
    const Pairs hits = GlossaryMatcher::match(entries, u("苹果手机很好"));
    EXPECT_EQ(keys(hits), QStringList{ u("苹果手机") });
}

TEST(GlossaryMatcher, ShorterTermOutsideTheLongerOneIsKept) {
    const Pairs entries{ { u("苹果"), u("apple") }, { u("苹果手机"), u("iPhone") } };
    const Pairs hits = GlossaryMatcher::match(entries, u("苹果手机和苹果"));
    EXPECT_EQ(hits.size(), 2);
}

TEST(GlossaryMatcher, MaxTermsCapsHowManyCandidatesAreAccepted) {
    Pairs entries;
    QString text;
    for (int i = 0; i < 20; ++i) {
        const QString term = QStringLiteral("词条") + QString(QChar(0x4e00 + i));
        entries.append({ term, QStringLiteral("t%1").arg(i) });
        text += term;
    }
    EXPECT_EQ(GlossaryMatcher::match(entries, text, 10).size(), 10);
}

TEST(GlossaryMatcher, LatinTermNeedsWholeWordBoundaries) {
    const Pairs entries{ { u("casa"), u("房子") } };
    EXPECT_TRUE(GlossaryMatcher::match(entries, u("el casamiento fue bonito")).isEmpty());
    EXPECT_EQ(GlossaryMatcher::match(entries, u("mi Casa es grande")).size(), 1);
}

TEST(GlossaryMatcher, LatinMultiWordTermKeepsItsSpaces) {
    const Pairs entries{ { u("carbon fiber"), u("碳纤维") } };
    EXPECT_EQ(GlossaryMatcher::match(entries, u("this carbon fiber frame")).size(), 1);
}

TEST(GlossaryMatcher, EmptyInputsAreSafe) {
    EXPECT_TRUE(GlossaryMatcher::match({}, u("abc")).isEmpty());
    EXPECT_TRUE(GlossaryMatcher::match({ { u("a"), u("b") } }, QString()).isEmpty());
    EXPECT_TRUE(GlossaryMatcher::match({ { QString(), u("b") } }, u("abc")).isEmpty());
}
