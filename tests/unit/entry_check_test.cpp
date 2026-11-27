#include <gtest/gtest.h>

#include "services/dictionary/EntryCheck.h"

using EntryCheck::Verdict;

TEST(EntryCheckTest, AnyLanguageRecognitionProfileNeverComplainsAndAddsNoTag) {
    const auto r = EntryCheck::recognition(QStringLiteral("科达"), QStringLiteral("Kuda"), QString());
    EXPECT_EQ(r.verdict, Verdict::Ok);
    EXPECT_TRUE(r.tag.isEmpty());
}

TEST(EntryCheckTest, RecognitionEntryInItsOwnLanguageGetsALanguageTag) {
    const auto r = EntryCheck::recognition(QStringLiteral("hola"), QStringLiteral("Hola"), QStringLiteral("es"));
    EXPECT_EQ(r.verdict, Verdict::Ok);
    EXPECT_EQ(r.tag, QStringLiteral("lang:es"));
}

TEST(EntryCheckTest, LatinBrandNamesAreFineInAChineseProfile) {
    const auto r = EntryCheck::recognition(QStringLiteral("科达"), QStringLiteral("Kuda"), QStringLiteral("zh"));
    EXPECT_EQ(r.verdict, Verdict::Ok);
    EXPECT_EQ(r.tag, QStringLiteral("lang:zh"));
}

TEST(EntryCheckTest, RecognitionEntryInAnotherScriptIsFlaggedWithoutATag) {
    const auto r = EntryCheck::recognition(QStringLiteral("科达"), QStringLiteral("Kuda"), QStringLiteral("es"));
    EXPECT_EQ(r.verdict, Verdict::Mismatch);
    EXPECT_TRUE(r.tag.isEmpty());
}

TEST(EntryCheckTest, TranslationEntryInTheRightColumnsGetsAPairTag) {
    const auto r = EntryCheck::translation(QStringLiteral("密钥"), QStringLiteral("clave"), QStringLiteral("zh"), QStringLiteral("es"));
    EXPECT_EQ(r.verdict, Verdict::Ok);
    EXPECT_EQ(r.tag, QStringLiteral("lang:zh>es"));
}

TEST(EntryCheckTest, SwappedColumnsAreDetectedByScript) {
    const auto r = EntryCheck::translation(QStringLiteral("clave"), QStringLiteral("密钥"), QStringLiteral("zh"), QStringLiteral("es"));
    EXPECT_EQ(r.verdict, Verdict::SwapSuggested);
}

TEST(EntryCheckTest, IdentityMappingsAndUncertainPairsAreAccepted) {
    EXPECT_EQ(EntryCheck::translation(QStringLiteral("密钥"), QStringLiteral("密钥"), QStringLiteral("zh"), QStringLiteral("es")).verdict, Verdict::Ok);
    const auto uncertain = EntryCheck::translation(QStringLiteral("hola"), QStringLiteral("hello"), QStringLiteral("es"), QStringLiteral("en"));
    EXPECT_EQ(uncertain.verdict, Verdict::Ok);
    EXPECT_EQ(uncertain.tag, QStringLiteral("lang:es>en"));
}

TEST(EntryCheckTest, ALatinCueCanRevealSwappedColumnsInASharedScriptPair) {
    const auto r = EntryCheck::translation(QStringLiteral("hello"), QStringLiteral("¿qué?"), QStringLiteral("es"), QStringLiteral("en"));
    EXPECT_EQ(r.verdict, Verdict::SwapSuggested);
}
