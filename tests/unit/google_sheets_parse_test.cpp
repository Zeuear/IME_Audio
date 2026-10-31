#include <gtest/gtest.h>

#include "snippets/GoogleSheetsService.h"

namespace {
QString u(const char* s) { return QString::fromUtf8(s); }
}

TEST(GoogleSheetsCsv, SimpleRows) {
    const auto rows = GoogleSheetsService::parseCsv("a,b\nc,d\n");
    ASSERT_EQ(rows.size(), 2);
    EXPECT_EQ(rows[0], (QStringList{ "a", "b" }));
    EXPECT_EQ(rows[1], (QStringList{ "c", "d" }));
}

// 引号里的逗号、换行和 "" 转义都是内容；CRLF 当一个换行
TEST(GoogleSheetsCsv, QuotedFieldsAndCrlf) {
    const auto rows = GoogleSheetsService::parseCsv("\"a,1\",\"line1\nline2\"\r\n\"say \"\"hi\"\"\",x\r\n");
    ASSERT_EQ(rows.size(), 2);
    EXPECT_EQ(rows[0][0], "a,1");
    EXPECT_EQ(rows[0][1], "line1\nline2");
    EXPECT_EQ(rows[1][0], "say \"hi\"");
}

TEST(GoogleSheetsCsv, EmptyInputAndBlankLines) {
    EXPECT_TRUE(GoogleSheetsService::parseCsv("").isEmpty());
    EXPECT_TRUE(GoogleSheetsService::parseCsv("\n\n").isEmpty());
}

TEST(GoogleSheetsId, ExtractsFromUrlAndBareId) {
    EXPECT_EQ(GoogleSheetsService::extractSpreadsheetId(
                  "https://docs.google.com/spreadsheets/d/1AbC-dEf_123/edit#gid=0"), "1AbC-dEf_123");
    EXPECT_EQ(GoogleSheetsService::extractSpreadsheetId("  1AbCdEfGhIjKlMnOpQ  "), "1AbCdEfGhIjKlMnOpQ");
    EXPECT_TRUE(GoogleSheetsService::extractSpreadsheetId("hello").isEmpty());
    EXPECT_TRUE(GoogleSheetsService::extractSpreadsheetId("").isEmpty());
}

TEST(GoogleSheetsParse, RowsBecomeItemsAndSkipHeader) {
    FolderNode l2;
    GoogleSheetsService::parseRowsIntoFolder(&l2, {
        { u("中文"), u("外文") },        // 表头，跳过
        { u("你好"), "Hello" },
        { "", "" },                       // 空行跳过
    });
    ASSERT_EQ(l2.items.size(), 1u);
    EXPECT_EQ(l2.items[0].title, u("你好"));
    EXPECT_EQ(l2.items[0].text, "Hello");
    EXPECT_EQ(l2.items[0].type, "text");
    EXPECT_FALSE(l2.items[0].id.isEmpty());
}

// A 列含 @@ 开新分组（L3），其后的条目归入该分组
TEST(GoogleSheetsParse, AtAtStartsGroup) {
    FolderNode l2;
    GoogleSheetsService::parseRowsIntoFolder(&l2, {
        { "top", "t" },
        { "@@Greetings", "" },
        { "hi", "Hello" },
    });
    ASSERT_EQ(l2.items.size(), 1u);
    ASSERT_EQ(l2.children.size(), 1u);
    EXPECT_EQ(l2.children[0]->title, "Greetings");
    EXPECT_EQ(l2.children[0]->level, 3);
    ASSERT_EQ(l2.children[0]->items.size(), 1u);
    EXPECT_EQ(l2.children[0]->items[0].title, "hi");
}

TEST(GoogleSheetsParse, ImageFormulaBecomesImagePath) {
    FolderNode l2;
    GoogleSheetsService::parseRowsIntoFolder(&l2, {
        { "pic", "=IMAGE(\"https://x.test/a.png\")" },
        { "both", "text" },
    }, { { { 1, 1 }, "images/b.png" } });
    ASSERT_EQ(l2.items.size(), 2u);
    EXPECT_EQ(l2.items[0].imagePath, "https://x.test/a.png");
    EXPECT_TRUE(l2.items[0].text.isEmpty());   // 公式本身不作为外文
    EXPECT_EQ(l2.items[0].type, "image");
    EXPECT_EQ(l2.items[1].imagePath, "images/b.png");   // 单元格里直接插入的图优先按位置对上
    EXPECT_EQ(l2.items[1].type, "combo");
}
