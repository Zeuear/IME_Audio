#include <gtest/gtest.h>
#include <QTime>

#include "ui/models/TranscriptLineModel.h"

TEST(TranscriptLineModel, EachUtteranceIsOneLineInSpokenOrder) {
    TranscriptLineModel m;
    m.add("今天下午开会。", QTime(14, 2, 11));
    m.add("OK, see you.", QTime(14, 2, 15));
    ASSERT_EQ(m.rowCount(), 2);
    EXPECT_EQ(m.textAt(0), "今天下午开会。");
    EXPECT_EQ(m.allText(false), "今天下午开会。\nOK, see you.");
    EXPECT_EQ(m.allText(true), "[14:02:11] 今天下午开会。\n[14:02:15] OK, see you.");
}

TEST(TranscriptLineModel, BlankUtteranceIsIgnored) {
    TranscriptLineModel m;
    m.add("   \n ");
    EXPECT_EQ(m.rowCount(), 0);
}

TEST(TranscriptLineModel, LineBreaksInsideAnUtteranceBecomeSpaces) {
    TranscriptLineModel m;
    m.add(" first\r\n  second \n", QTime(1, 0, 0));
    EXPECT_EQ(m.textAt(0), "first second");
}

TEST(TranscriptLineModel, SelectedRowsAreCopiedInGivenOrder) {
    TranscriptLineModel m;
    m.add("a", QTime(1, 0, 0));
    m.add("b", QTime(1, 0, 1));
    m.add("c", QTime(1, 0, 2));
    EXPECT_EQ(m.linesText({ 0, 2 }, false), "a\nc");
    EXPECT_EQ(m.linesText({ 1, 9 }, true), "[01:00:01] b");
}

TEST(TranscriptLineModel, SerializedTranscriptRestoresWithTimes) {
    TranscriptLineModel src;
    src.add("你好。", QTime(9, 30, 0));
    src.add("Hello.", QTime(9, 30, 5));

    TranscriptLineModel dst;
    dst.restore(src.serialize());
    ASSERT_EQ(dst.rowCount(), 2);
    EXPECT_EQ(dst.data(dst.index(1), TranscriptLineModel::TimeRole).toString(), "09:30:05");
    EXPECT_EQ(dst.allText(true), src.allText(true));
}

TEST(TranscriptLineModel, OldPlainTranscriptRestoresLineByLineWithoutTime) {
    TranscriptLineModel m;
    m.restore("整段旧稿子。第二句。\n\n[10:00:00] 带时间的一行\n[bad] 不是时间");
    ASSERT_EQ(m.rowCount(), 3);
    EXPECT_EQ(m.textAt(0), "整段旧稿子。第二句。");
    EXPECT_TRUE(m.data(m.index(0), TranscriptLineModel::TimeRole).toString().isEmpty());
    EXPECT_EQ(m.textAt(1), "带时间的一行");
    EXPECT_EQ(m.textAt(2), "[bad] 不是时间");
}

TEST(TranscriptLineModel, ClearRemovesEverything) {
    TranscriptLineModel m;
    m.add("x");
    m.clear();
    EXPECT_EQ(m.rowCount(), 0);
    EXPECT_TRUE(m.serialize().isEmpty());
}
