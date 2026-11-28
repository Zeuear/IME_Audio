#include <gtest/gtest.h>

#include <QJsonObject>
#include <QTemporaryDir>

#include "services/TermsLibraryManager.h"
#include "services/dictionary/GlossaryImport.h"

using namespace GlossaryImport;

namespace {

Parsed lines(const QString& text, bool requireHeard = true) {
    return parseText(text, requireHeard);
}

}

TEST(GlossaryImportTest, AuraproGlossaryFileCarriesMetadataAndEntries) {
    const QString json = QStringLiteral(R"({
        "format": "aurapro-glossary", "format_version": 1,
        "glossary": {"id": "zh-bg", "name": "中文-保加利亚语词典", "version": "1.0.0",
                     "source_lang": "中文", "glossary_lang": "保加利亚语", "target_lang": "保加利亚语"},
        "entries": {"神": "Бог", "耶稣": "Исус"}})");
    const Parsed p = lines(json);
    EXPECT_EQ(p.format, "aurapro-glossary");
    EXPECT_EQ(p.name, "中文-保加利亚语词典");
    EXPECT_EQ(p.version, "1.0.0");
    EXPECT_EQ(p.sourceLang, "中文");
    EXPECT_EQ(p.glossaryLang, "保加利亚语");
    ASSERT_EQ(p.entries.size(), 2);
    EXPECT_TRUE(p.issues.isEmpty());
}

TEST(GlossaryImportTest, PlainJsonMapAndArrays) {
    EXPECT_EQ(lines(R"({"GPU": "显卡", "CPU": "处理器"})").entries.size(), 2);
    const Parsed objs = lines(R"([{"source": "GPU", "target": "显卡"}, {"heard": "k8s", "correct": "Kubernetes"}])");
    ASSERT_EQ(objs.entries.size(), 2);
    EXPECT_EQ(objs.entries[1].correct, "Kubernetes");
    const Parsed pairs = lines(R"([["GPU", "显卡"], ["CPU", "处理器"]])");
    ASSERT_EQ(pairs.entries.size(), 2);
    EXPECT_EQ(pairs.entries[0].heard, "GPU");
}

TEST(GlossaryImportTest, CommonSeparatorsAreRecognised) {
    const Parsed p = lines(QStringLiteral(
        "a1 => b1\n"
        "a2 -> b2\n"
        "a3→b3\n"
        "a4 = b4\n"
        "a5\tb5\n"
        "a6, b6\n"
        "a7: b7\n"
        "a8：b8\n"
        "a9|b9\n"));
    ASSERT_EQ(p.entries.size(), 9);
    ASSERT_TRUE(p.issues.isEmpty());
    for (int i = 0; i < 9; ++i) {
        EXPECT_EQ(p.entries[i].heard, QStringLiteral("a%1").arg(i + 1));
        EXPECT_EQ(p.entries[i].correct, QStringLiteral("b%1").arg(i + 1));
    }
}

TEST(GlossaryImportTest, StrongSeparatorBeatsCommasInTheHeardColumn) {
    const Parsed p = lines(QStringLiteral("k8s, 开八斯 => Kubernetes"));
    ASSERT_EQ(p.entries.size(), 1);
    EXPECT_EQ(p.entries[0].heard, "k8s, 开八斯");
    EXPECT_EQ(p.entries[0].correct, "Kubernetes");
}

TEST(GlossaryImportTest, SeveralRulesOnOneLine) {
    const Parsed p = lines(QStringLiteral("a=>b;c=>d；e=>f"));
    ASSERT_EQ(p.entries.size(), 3);
    EXPECT_EQ(p.entries[2].correct, "f");
}

TEST(GlossaryImportTest, QuotedCsvKeepsCommasInsideFields) {
    const Parsed p = lines(QStringLiteral("\"hello, world\",\"你好,世界\""));
    ASSERT_EQ(p.entries.size(), 1);
    EXPECT_EQ(p.entries[0].heard, "hello, world");
    EXPECT_EQ(p.entries[0].correct, "你好,世界");
}

TEST(GlossaryImportTest, BlankCommentAndHeaderLinesAreSkipped) {
    const Parsed p = lines(QStringLiteral("source\ttarget\n\n# comment\n// another\nGPU\t显卡\n"));
    ASSERT_EQ(p.entries.size(), 1);
    EXPECT_TRUE(p.issues.isEmpty());
}

TEST(GlossaryImportTest, TranslationLineWithoutSeparatorIsReportedWithItsLineNumber) {
    const Parsed p = lines(QStringLiteral("GPU => 显卡\nnonsense\nCPU => 处理器"));
    EXPECT_EQ(p.entries.size(), 2);
    ASSERT_EQ(p.issues.size(), 1);
    EXPECT_EQ(p.issues[0].line, 2);
}

TEST(GlossaryImportTest, RecognitionAllowsASingleWordAsHotword) {
    const Parsed p = lines(QStringLiteral("Aurapro\nGPT => GPT-4"), false);
    ASSERT_EQ(p.entries.size(), 2);
    EXPECT_TRUE(p.entries[0].heard.isEmpty());
    EXPECT_EQ(p.entries[0].correct, "Aurapro");
}

TEST(GlossaryImportTest, ExportedTsvKeepsEnabledFlagAliasesAndNotes) {
    const Parsed p = lines(QStringLiteral("0\twrong\tright\talias1, alias2\tall\ttag\tnote\n1\tx\ty\t\tall\t\t\n"));
    EXPECT_EQ(p.format, "tsv");
    ASSERT_EQ(p.entries.size(), 2);
    EXPECT_FALSE(p.entries[0].enabled);
    EXPECT_EQ(p.entries[0].heard, "wrong, alias1, alias2");
    EXPECT_EQ(p.entries[0].notes, "note");
    EXPECT_TRUE(p.entries[1].enabled);
}

TEST(GlossaryImportTest, BomAndCrlfAreHandled) {
    const Parsed p = lines(QString(QChar(0xFEFF)) + QStringLiteral("a=>b\r\nc=>d\r\n"));
    EXPECT_EQ(p.entries.size(), 2);
}

TEST(GlossaryImportTest, BracketsKeepSeparatorsInsideOneWord) {
    EXPECT_EQ(TermsLibraryManager::splitWords(QStringLiteral("触犯（诫命、条例）")), QStringList{ "触犯（诫命、条例）" });
    EXPECT_EQ(TermsLibraryManager::splitWords(QStringLiteral("修理/对付/修理对付")), (QStringList{ "修理", "对付", "修理对付" }));
    EXPECT_EQ(TermsLibraryManager::splitWords(QStringLiteral("a(b/c), d")), (QStringList{ "a(b/c)", "d" }));
    // 括号没闭合时不吞掉后面的内容
    EXPECT_EQ(TermsLibraryManager::splitWords(QStringLiteral("a(b, c")), (QStringList{ "a(b", "c" }));
}

class GlossaryMergeTest : public ::testing::Test {
protected:
    QTemporaryDir dir;
    TermsLibraryManager lib{ dir.filePath("terms.tsv") };
};

TEST_F(GlossaryMergeTest, AddsAllEntriesWithOneSave) {
    int reloads = 0;
    QObject::connect(&lib, &TermsLibraryManager::termsReloaded, [&]() { ++reloads; });
    const Parsed p = lines(QStringLiteral("a=>1\nb=>2\nc=>3"));
    const Summary s = mergeInto(lib, p.entries, true, QString(), QString(), false);
    EXPECT_EQ(s.added, 3);
    EXPECT_EQ(lib.items().size(), 3);
    EXPECT_EQ(reloads, 1);
}

TEST_F(GlossaryMergeTest, SkipsDuplicatesWithinBatchAndAgainstExistingTerms) {
    TermItem existing;
    existing.wrong = "gpu";
    existing.correct = "显卡";
    lib.addItem(existing);
    const Parsed p = lines(QStringLiteral("GPU=>显卡2\nnew=>新\nNEW=>新2"));
    const Summary s = mergeInto(lib, p.entries, true, QString(), QString(), false);
    EXPECT_EQ(s.added, 1);
    EXPECT_EQ(s.duplicates, 2);
    EXPECT_EQ(lib.items().size(), 2);
}

TEST_F(GlossaryMergeTest, SwapExchangesTheTwoColumns) {
    const Parsed p = lines(QStringLiteral("显卡=>GPU"));
    mergeInto(lib, p.entries, true, QString(), QString(), true);
    ASSERT_EQ(lib.items().size(), 1);
    EXPECT_EQ(lib.items()[0].wrong, "GPU");
    EXPECT_EQ(lib.items()[0].correct, "显卡");
}

TEST_F(GlossaryMergeTest, EntriesWithSeparatorsInsideBracketsStayOneTerm) {
    const Parsed p = lines(QStringLiteral("触犯（诫命、条例）=>нарушавам"));
    mergeInto(lib, p.entries, true, QString(), QString(), false);
    ASSERT_EQ(lib.items().size(), 1);
    EXPECT_EQ(lib.items()[0].heard(), QStringList{ "触犯（诫命、条例）" });
}

TEST_F(GlossaryMergeTest, HotwordOnlyEntriesAreNotDuplicated) {
    const Parsed p = lines(QStringLiteral("Aurapro\nAurapro"), false);
    const Summary s = mergeInto(lib, p.entries, false, QString(), QString(), false);
    EXPECT_EQ(s.added, 1);
    EXPECT_EQ(s.duplicates, 1);
}
