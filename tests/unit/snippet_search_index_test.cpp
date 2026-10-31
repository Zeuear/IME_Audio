#include <gtest/gtest.h>

#include "snippets/SnippetSearchIndex.h"

TEST(SnippetExactKeyword, IgnoresPunctuationSpaceAndCase) {
    EXPECT_TRUE(SnippetSearchIndex::isExactKeyword(QString::fromUtf8("报价。"), QString::fromUtf8("报价")));
    EXPECT_TRUE(SnippetSearchIndex::isExactKeyword(QString::fromUtf8(" 报 价！"), QString::fromUtf8("报价")));
    EXPECT_TRUE(SnippetSearchIndex::isExactKeyword("Quote.", "quote"));
}

TEST(SnippetExactKeyword, SentenceContainingKeywordIsNotExact) {
    EXPECT_FALSE(SnippetSearchIndex::isExactKeyword(QString::fromUtf8("给我报价。"), QString::fromUtf8("报价")));
    EXPECT_FALSE(SnippetSearchIndex::isExactKeyword("quote this", "quote"));
}

// 只由符号组成的快捷码归一化后为空，不能让空句子或纯标点命中
TEST(SnippetExactKeyword, EmptyAfterNormalizingNeverMatches) {
    EXPECT_FALSE(SnippetSearchIndex::isExactKeyword("//", "//"));
    EXPECT_FALSE(SnippetSearchIndex::isExactKeyword("", ""));
    EXPECT_FALSE(SnippetSearchIndex::isExactKeyword(QString::fromUtf8("。"), QString::fromUtf8("报价")));
}
