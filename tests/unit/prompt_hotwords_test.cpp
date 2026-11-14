#include <gtest/gtest.h>

#include "sherpa/PromptHotwords.h"

TEST(PromptHotwords, JoinsLinesWithAsciiComma)
{
    const auto r = PromptHotwords::build(QStringLiteral("张三\nImeAudio\n\n  语音输入法  \n"));
    EXPECT_EQ(r.csv, QStringLiteral("张三,ImeAudio,语音输入法"));
    EXPECT_EQ(r.used, 3);
    EXPECT_EQ(r.dropped, 0);
}

TEST(PromptHotwords, EmptyInputGivesEmptyCsv)
{
    const auto r = PromptHotwords::build(QStringLiteral("\n  \n"));
    EXPECT_TRUE(r.csv.isEmpty());
    EXPECT_EQ(r.used, 0);
}

// 英文逗号是分隔符，词里自带的逗号不能把一个词拆成两个
TEST(PromptHotwords, CommaInsideWordBecomesSpace)
{
    const auto r = PromptHotwords::build(QStringLiteral("Hello, World"));
    EXPECT_EQ(r.csv, QStringLiteral("Hello World"));
}

TEST(PromptHotwords, DuplicatesAreSkipped)
{
    const auto r = PromptHotwords::build(QStringLiteral("张三\n张三\n李四"));
    EXPECT_EQ(r.csv, QStringLiteral("张三,李四"));
}

TEST(PromptHotwords, EstimateCountsCjkPerCharAndAsciiPerFourChars)
{
    EXPECT_EQ(PromptHotwords::estimateTokens(QStringLiteral("张三")), 3);       // 2 + 分隔符
    EXPECT_EQ(PromptHotwords::estimateTokens(QStringLiteral("ImeAudio")), 3);   // 8 字母 = 2 + 分隔符
    EXPECT_EQ(PromptHotwords::estimateTokens(QStringLiteral("GPT-4o")), 4);     // GPT, -, 4o + 分隔符
}

// 超出预算后按词典顺序截断：前面的词全用上，后面的一律不用，即使后面有更短的词
TEST(PromptHotwords, StopsAtBudgetKeepingDictionaryOrder)
{
    const auto r = PromptHotwords::build(QStringLiteral("一二三四\n五六七八\n九\n十"), 9);
    EXPECT_EQ(r.csv, QStringLiteral("一二三四"));
    EXPECT_EQ(r.used, 1);
    EXPECT_EQ(r.dropped, 3);
    EXPECT_EQ(r.tokens, 5);
}

TEST(PromptHotwords, DefaultBudgetStaysUnderSherpaWarningThreshold)
{
    QString lines;
    for (int i = 0; i < 200; ++i) lines += QStringLiteral("专有名词%1\n").arg(i);
    const auto r = PromptHotwords::build(lines);
    EXPECT_GT(r.used, 0);
    EXPECT_GT(r.dropped, 0);
    EXPECT_LE(r.tokens, PromptHotwords::kMaxTokens);
    EXPECT_EQ(r.used + r.dropped, 200);
}
