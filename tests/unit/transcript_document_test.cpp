#include <gtest/gtest.h>
#include <QDateTime>
#include <QTextCursor>
#include <QTextDocument>

#include "ui/models/TranscriptDocument.h"

namespace {
QDateTime at(int h, int m, int s) { return QDateTime(QDate(2026, 11, 7), QTime(h, m, s)); }
} // namespace

TEST(TranscriptDocument, ChineseSentencesJoinWithoutSeparator) {
    QTextDocument doc;
    TranscriptDocument t(&doc);
    t.appendUtterance("你好。", at(9, 0, 0));
    t.appendUtterance("今天天气不错。", at(9, 0, 5));
    EXPECT_EQ(doc.toPlainText(), "你好。今天天气不错。");
}

TEST(TranscriptDocument, LatinSentencesAreSeparatedBySpace) {
    QTextDocument doc;
    TranscriptDocument t(&doc);
    t.appendUtterance("Hello world.", at(9, 0, 0));
    t.appendUtterance("How are you?", at(9, 0, 5));
    EXPECT_EQ(doc.toPlainText(), "Hello world. How are you?");
}

TEST(TranscriptDocument, BlankUtteranceIsIgnored) {
    QTextDocument doc;
    TranscriptDocument t(&doc);
    t.appendUtterance("  ", at(9, 0, 0));
    t.appendUtterance("a", at(9, 0, 1));
    EXPECT_EQ(doc.toPlainText(), "a");
}

TEST(TranscriptDocument, TimestampsPutEachUtteranceOnItsOwnLine) {
    QTextDocument doc;
    TranscriptDocument t(&doc);
    t.setTimestampsVisible(true);
    t.appendUtterance("第一句。", at(9, 0, 3));
    t.appendUtterance("第二句。", at(9, 1, 20));
    EXPECT_EQ(doc.toPlainText(), "[09:00:03] 第一句。\n[09:01:20] 第二句。");
}

TEST(TranscriptDocument, TimestampSwitchOnlyAffectsLaterUtterances) {
    // 已写进稿子（可能被用户改过）的内容不回头重排
    QTextDocument doc;
    TranscriptDocument t(&doc);
    t.appendUtterance("甲。", at(9, 0, 0));
    t.setTimestampsVisible(true);
    t.appendUtterance("乙。", at(9, 0, 9));
    EXPECT_EQ(doc.toPlainText(), "甲。\n[09:00:09] 乙。");
}

TEST(TranscriptDocument, UserEditsInEarlierTextDoNotMoveTheAppendPoint) {
    QTextDocument doc;
    TranscriptDocument t(&doc);
    t.appendUtterance("今天开会。", at(9, 0, 0));

    // 用户在开头补了一段，又在中间改了字
    QTextCursor user(&doc);
    user.setPosition(0);
    user.insertText("【标题】");
    user.setPosition(5);
    user.insertText("下午");

    t.appendUtterance("讨论预算。", at(9, 0, 8));
    EXPECT_EQ(doc.toPlainText(), "【标题】今下午天开会。讨论预算。");
}

TEST(TranscriptDocument, ClearedDocumentStartsFreshWithoutLeadingSpace) {
    QTextDocument doc;
    TranscriptDocument t(&doc);
    t.appendUtterance("Hello.", at(9, 0, 0));
    doc.clear();   // 用户全选删除
    t.appendUtterance("Again.", at(9, 0, 5));
    EXPECT_EQ(doc.toPlainText(), "Again.");
}

TEST(TranscriptDocument, TrailingNewlineFromUserCountsAsLineStart) {
    // 用户在末尾按了回车另起一段：新内容紧接其后，不再多补空格
    QTextDocument doc;
    TranscriptDocument t(&doc);
    t.appendUtterance("Hello.", at(9, 0, 0));
    QTextCursor user(&doc);
    user.movePosition(QTextCursor::End);
    user.insertText("\n");
    t.appendUtterance("Next.", at(9, 0, 5));
    EXPECT_EQ(doc.toPlainText(), "Hello.\nNext.");
}
