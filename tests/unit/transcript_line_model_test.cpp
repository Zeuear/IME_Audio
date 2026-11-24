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

TEST(TranscriptLineModel, TranslationSitsBesideTheOriginalAndIsEmptyWithoutTranslation) {
    TranscriptLineModel m;
    m.add("Hola", QTime(1, 0, 0), "你好");
    m.add("Adios", QTime(1, 0, 1));
    ASSERT_EQ(m.rowCount(), 2);
    EXPECT_EQ(m.textAt(0), "Hola");   // 左栏是原文
    EXPECT_EQ(m.data(m.index(0), TranscriptLineModel::TranslationRole).toString(), "你好");
    EXPECT_TRUE(m.data(m.index(1), TranscriptLineModel::TranslationRole).toString().isEmpty());   // 右栏对应空行
    EXPECT_TRUE(m.hasTranslation());
}

TEST(TranscriptLineModel, TranslationIsASeparateTextAndSurvivesSaveAndRestore) {
    TranscriptLineModel m;
    m.add("Hola", QTime(1, 0, 0), "你\n好");
    m.add("Adios", QTime(1, 0, 1));
    EXPECT_EQ(m.serialize(), "[01:00:00] Hola\n>> 你 好\n[01:00:01] Adios");   // 译文随稿子存盘
    EXPECT_EQ(m.allText(false), "Hola\nAdios");              // 原文里没有译文
    EXPECT_EQ(m.allTranslationText(false), "你 好");           // 译文单独成文，没有译文的句子跳过
    EXPECT_TRUE(m.translationLinesText({ 1 }, false).isEmpty());
    EXPECT_EQ(m.translationAt(0), "你 好");

    TranscriptLineModel none;
    none.add("a", QTime(1, 0, 0));
    EXPECT_FALSE(none.hasTranslation());
    EXPECT_EQ(none.allTranslationText(true), QString());
}

TEST(TranscriptLineModel, RestoreKeepsTranslationsAndReadsOldFormat) {
    TranscriptLineModel m;
    m.add("Hola", QTime(1, 0, 0), "你好");
    m.add("Adios", QTime(1, 0, 1));
    m.add("Gracias", QTime(1, 0, 2), "谢谢");

    TranscriptLineModel back;
    back.restore(m.serialize());
    ASSERT_EQ(back.rowCount(), 3);
    EXPECT_EQ(back.translationAt(0), "你好");
    EXPECT_TRUE(back.translationAt(1).isEmpty());
    EXPECT_EQ(back.translationAt(2), "谢谢");
    EXPECT_EQ(back.textAt(2), "Gracias");

    TranscriptLineModel old;   // 老版本存的稿子没有译文行
    old.restore("[01:00:00] a\n[01:00:01] b");
    EXPECT_EQ(old.rowCount(), 2);
    EXPECT_FALSE(old.hasTranslation());
}

TEST(TranscriptLineModel, ManualEditChangesTextAndTranslationAndEmitsDataChanged) {
    TranscriptLineModel m;
    m.add("Hola", QTime(1, 0, 0), "你好");
    int changes = 0;
    QObject::connect(&m, &QAbstractItemModel::dataChanged, &m, [&changes]() { ++changes; });
    EXPECT_TRUE(m.setText(0, " Hola amigo\n"));
    EXPECT_EQ(m.textAt(0), "Hola amigo");
    EXPECT_TRUE(m.setTranslation(0, "你好朋友"));
    EXPECT_EQ(changes, 2);
    EXPECT_FALSE(m.setText(0, "Hola amigo"));   // 没变化不发信号
    EXPECT_FALSE(m.setText(0, "   "));          // 原文不能改成空
    EXPECT_EQ(m.textAt(0), "Hola amigo");
    EXPECT_TRUE(m.setTranslation(0, ""));       // 译文可以清空
    EXPECT_FALSE(m.hasTranslation());
    EXPECT_FALSE(m.setText(5, "x"));
}

TEST(TranscriptLineModel, PendingRowShowsOriginalFirstThenGetsTranslationInOrder) {
    TranscriptLineModel m;
    m.add("旧的一句", QTime(1, 0, 0));
    m.addPending(7, "Hola", QTime(1, 0, 1));
    m.addPending(5, "Antes", QTime(1, 0, 2));   // 序号小的排前面，即使晚到
    m.addPending(9, "Fin", QTime(1, 0, 3));
    ASSERT_EQ(m.rowCount(), 4);
    EXPECT_EQ(m.textAt(1), "Antes");
    EXPECT_EQ(m.textAt(2), "Hola");
    EXPECT_EQ(m.textAt(3), "Fin");
    EXPECT_TRUE(m.translationAt(2).isEmpty());   // 译文还没回来，右栏先空着

    EXPECT_TRUE(m.completePending(7, "Hola!", "你好"));
    EXPECT_EQ(m.textAt(2), "Hola!");
    EXPECT_EQ(m.translationAt(2), "你好");
    EXPECT_FALSE(m.completePending(7, "x", "y"));   // 已补完的不能再补，找不到
    EXPECT_FALSE(m.completePending(42, "x", "y"));
}

TEST(TranscriptLineModel, CompletingPendingDoesNotOverwriteUserEditedOriginal) {
    TranscriptLineModel m;
    m.addPending(1, "Hola", QTime(1, 0, 0));
    ASSERT_TRUE(m.setText(0, "Hola amigo"));      // 用户在等译文时手动改了原文
    EXPECT_TRUE(m.completePending(1, "Hola!", "你好"));
    EXPECT_EQ(m.textAt(0), "Hola amigo");         // 不覆盖用户的修改
    EXPECT_EQ(m.translationAt(0), "你好");
    EXPECT_EQ(m.serialize(), "[01:00:00] Hola amigo\n>> 你好");
}
