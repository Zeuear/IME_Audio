#include <gtest/gtest.h>

#include <memory>

#include "snippets/SnippetSearchIndex.h"

namespace {

SnippetItem makeItem(const char* id, const QString& title, const QString& keyword = {}) {
    SnippetItem it;
    it.id = id;
    it.title = title;
    it.keyword = keyword;
    return it;
}

std::unique_ptr<FolderNode> makeFolder(const QString& title) {
    auto f = std::make_unique<FolderNode>();
    f->title = title;
    return f;
}

// 主表 / Sheet / 分组 三层，每层各一条
FolderList sampleTree() {
    auto l3 = makeFolder("Group");
    l3->items.push_back(makeItem("c", "gamma", "gk"));
    auto l2 = makeFolder("Sheet");
    l2->items.push_back(makeItem("b", "beta", "bk"));
    l2->children.push_back(std::move(l3));
    auto l1 = makeFolder("Book");
    l1->items.push_back(makeItem("a", "alpha", "ak"));
    l1->children.push_back(std::move(l2));
    FolderList roots;
    roots.push_back(std::move(l1));
    return roots;
}

} // namespace

// 命中顺序与树一致（先本目录条目，再子目录）：搜索弹窗按目录路径变化插入目录行，依赖这个顺序
TEST(SnippetSearch, HitsFollowTreeOrderWithTrail) {
    FolderList roots = sampleTree();
    SnippetSearchIndex index;
    const auto hits = index.search(roots, "", true);
    ASSERT_EQ(hits.size(), 3);
    EXPECT_EQ(hits[0].item->id, "a");
    EXPECT_EQ(hits[0].trail, "Book");
    EXPECT_EQ(hits[1].item->id, "b");
    EXPECT_EQ(hits[1].trail, "Book / Sheet");
    EXPECT_EQ(hits[2].item->id, "c");
    EXPECT_EQ(hits[2].trail, "Book / Sheet / Group");
}

// 同名的相邻三级目录要靠 id 区分，弹窗按 trailKeys 分组，不能合并成一个目录行
TEST(SnippetSearch, SameNameSiblingFoldersHaveDistinctKeys) {
    auto g1 = makeFolder("Group");
    g1->id = "g1";
    g1->items.push_back(makeItem("x", "one"));
    auto g2 = makeFolder("Group");
    g2->id = "g2";
    g2->items.push_back(makeItem("y", "two"));
    auto sheet = makeFolder("Sheet");
    sheet->id = "s";
    sheet->children.push_back(std::move(g1));
    sheet->children.push_back(std::move(g2));
    FolderList roots;
    roots.push_back(std::move(sheet));

    SnippetSearchIndex index;
    const auto hits = index.search(roots, "", true);
    ASSERT_EQ(hits.size(), 2);
    EXPECT_EQ(hits[0].trailNames, hits[1].trailNames);
    EXPECT_NE(hits[0].trailKeys, hits[1].trailKeys);
    EXPECT_EQ(hits[0].trailKeys.first(), hits[1].trailKeys.first());
}

TEST(SnippetSearch, EmptyQueryReturnsNothingUnlessAllRequested) {
    FolderList roots = sampleTree();
    SnippetSearchIndex index;
    EXPECT_TRUE(index.search(roots, "  ", false).isEmpty());
    EXPECT_EQ(index.search(roots, "", true).size(), 3);
}

// 目录名也参与搜索：输入分组名能找出其下的条目
TEST(SnippetSearch, MatchesFolderNameAndIsCaseInsensitive) {
    FolderList roots = sampleTree();
    SnippetSearchIndex index;
    const auto hits = index.search(roots, "GROUP", true);
    ASSERT_EQ(hits.size(), 1);
    EXPECT_EQ(hits[0].item->id, "c");
    EXPECT_TRUE(index.search(roots, "zzz-not-here", true).isEmpty());
}

// 树增删节点后必须 invalidate，否则索引里还是旧指针
TEST(SnippetSearch, InvalidateRebuildsAfterTreeChange) {
    FolderList roots = sampleTree();
    SnippetSearchIndex index;
    ASSERT_EQ(index.search(roots, "", true).size(), 3);
    roots[0]->items.push_back(makeItem("d", "delta"));
    index.invalidate();
    EXPECT_EQ(index.search(roots, "", true).size(), 4);
}

TEST(SnippetKeywordMatch, FindsSpokenKeywordAndRanksByUseCount) {
    FolderList roots = sampleTree();
    roots[0]->items[0].keyword = "quote";
    roots[0]->children[0]->items[0].keyword = "quote";
    roots[0]->children[0]->items[0].useCount = 5;
    SnippetSearchIndex index;
    const auto hits = index.matchKeywords(roots, "please quote now", 10);
    ASSERT_EQ(hits.size(), 2);
    EXPECT_EQ(hits[0]->id, "b");   // 用得多的排前面
    EXPECT_EQ(index.matchKeywords(roots, "please quote now", 1).size(), 1);
}

// 英文快捷码按词匹配：quote 不能命中 quotes
TEST(SnippetKeywordMatch, EnglishKeywordMatchesWholeWordOnly) {
    FolderList roots = sampleTree();
    roots[0]->items[0].keyword = "quote";
    SnippetSearchIndex index;
    EXPECT_TRUE(index.matchKeywords(roots, "many quotes here", 10).isEmpty());
    EXPECT_EQ(index.matchKeywords(roots, "a quote.", 10).size(), 1);
}

// 流式识别在词中间插标点 / 空格时仍要命中
TEST(SnippetKeywordMatch, PunctuationInsideKeywordIsIgnored) {
    FolderList roots = sampleTree();
    roots[0]->items[0].keyword = QString::fromUtf8("报价");
    SnippetSearchIndex index;
    EXPECT_EQ(index.matchKeywords(roots, QString::fromUtf8("给我报，价。"), 10).size(), 1);
    EXPECT_EQ(index.matchKeywords(roots, QString::fromUtf8("给我 报 价"), 10).size(), 1);
}

TEST(SnippetKeywordMatch, BlankSpokenTextOrBlankKeywordNeverMatches) {
    FolderList roots = sampleTree();
    roots[0]->items[0].keyword = "   ";
    SnippetSearchIndex index;
    EXPECT_TRUE(index.matchKeywords(roots, "   ", 10).isEmpty());
    EXPECT_TRUE(index.matchKeywords(roots, "alpha", 10).isEmpty());   // 空白快捷码不参与匹配
}

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
