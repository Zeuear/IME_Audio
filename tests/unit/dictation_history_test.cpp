#include <gtest/gtest.h>
#include "widgets/DictationHistoryModel.h"

TEST(DictationHistory, NewestFirstAndBlankIgnored) {
    DictationHistoryModel m;
    m.add("first");
    m.add("   ");
    m.add(" second ");
    ASSERT_EQ(m.rowCount(), 2);
    EXPECT_EQ(m.textAt(0), "second");
    EXPECT_EQ(m.textAt(1), "first");
    EXPECT_EQ(m.textAt(5), "");
}

TEST(DictationHistory, KeepsOnlyMostRecentEntries) {
    DictationHistoryModel m;
    for (int i = 0; i < DictationHistoryModel::kMaxEntries + 20; ++i) m.add(QString::number(i));
    EXPECT_EQ(m.rowCount(), DictationHistoryModel::kMaxEntries);
    EXPECT_EQ(m.textAt(0), QString::number(DictationHistoryModel::kMaxEntries + 19));
    EXPECT_EQ(m.textAt(DictationHistoryModel::kMaxEntries - 1), "20");
}

TEST(DictationHistory, AllTextIsOldestFirstOneLinePerEntry) {
    DictationHistoryModel m;
    EXPECT_TRUE(m.allText().isEmpty());
    m.add("first");
    m.add("second");
    m.add("third");
    // 列表里最新的在上；复制出去按说话顺序，从早到晚
    EXPECT_EQ(m.allText(), "first\nsecond\nthird");
}

TEST(DictationHistory, ClearEmptiesTheList) {
    DictationHistoryModel m;
    m.add("x");
    m.clear();
    EXPECT_EQ(m.rowCount(), 0);
}
