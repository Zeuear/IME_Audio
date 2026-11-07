#include <gtest/gtest.h>

#include "utils/DownloadText.h"

// ---- 大小换算 ----

TEST(DownloadText, SizesUseTheLargestFittingUnit) {
    EXPECT_EQ(DownloadText::formatSize(0), "0 B");
    EXPECT_EQ(DownloadText::formatSize(1023), "1023 B");
    EXPECT_EQ(DownloadText::formatSize(1024), "1.0 KB");
    EXPECT_EQ(DownloadText::formatSize(1536), "1.5 KB");
    EXPECT_EQ(DownloadText::formatSize(1024 * 1024), "1.0 MB");
    EXPECT_EQ(DownloadText::formatSize(5 * 1024 * 1024 + 512 * 1024), "5.5 MB");
    EXPECT_EQ(DownloadText::formatSize(qint64(2.4 * 1024 * 1024 * 1024)), "2.40 GB");
}

TEST(DownloadText, NegativeSizeMeansUnknown) {
    EXPECT_EQ(DownloadText::formatSize(-1), "?");
}

// ---- 进度说明行："文件名 · 第几个/共几个 · 百分比" ----

TEST(DownloadText, FullLineHasNameIndexAndPercent) {
    EXPECT_EQ(DownloadText::progressLine("model.int8.onnx", 3, 6, 45), "model.int8.onnx · 3/6 · 45%");
}

TEST(DownloadText, SingleFileHasNoIndex) {
    EXPECT_EQ(DownloadText::progressLine("model.onnx", 1, 1, 45), "model.onnx · 45%");
    EXPECT_EQ(DownloadText::progressLine("model.onnx", 1, 0, 45), "model.onnx · 45%");
}

TEST(DownloadText, UnknownPercentIsLeftOut) {
    EXPECT_EQ(DownloadText::progressLine("model.int8.onnx", 3, 6, -1), "model.int8.onnx · 3/6");
}

TEST(DownloadText, PercentIsClampedToZeroToHundred) {
    EXPECT_EQ(DownloadText::progressLine("a.bin", 1, 2, 150), "a.bin · 1/2 · 100%");
    EXPECT_EQ(DownloadText::progressLine("a.bin", 1, 2, -7), "a.bin · 1/2");
}

TEST(DownloadText, EmptyNameStillShowsIndexAndPercent) {
    EXPECT_EQ(DownloadText::progressLine("", 3, 6, 45), "3/6 · 45%");
}

TEST(DownloadText, LongNamesAreElidedInTheMiddleKeepingTheExtension) {
    const QString line = DownloadText::progressLine("sherpa-onnx-very-long-model-name-for-testing.int8.onnx", 2, 4, 10, 24);
    const QString name = line.section(" · ", 0, 0);
    EXPECT_LE(name.size(), 24);
    EXPECT_TRUE(name.contains(QChar(0x2026)));
    EXPECT_TRUE(name.endsWith("onnx"));
    EXPECT_TRUE(name.startsWith("sherpa"));
    EXPECT_TRUE(line.endsWith(" · 2/4 · 10%"));
}

TEST(DownloadText, NamesThatFitAreLeftAlone) {
    EXPECT_EQ(DownloadText::progressLine("tokens.txt", 1, 3, 5, 24), "tokens.txt · 1/3 · 5%");
}

// ---- 一组文件的进度累计：几个文件、完成几个、当前文件、整体百分比 ----

TEST(DownloadGroupProgress, StartsEmpty) {
    DownloadGroupProgress g;
    EXPECT_EQ(g.percent(), 0);
    EXPECT_TRUE(g.line().isEmpty());
}

TEST(DownloadGroupProgress, ShowsTheCurrentFileOfTheGroup) {
    DownloadGroupProgress g;
    g.start(6);
    g.update("a.onnx", 10);
    EXPECT_EQ(g.line(), "a.onnx · 1/6 · 10%");
    g.fileFinished();
    g.fileFinished();
    g.update("c.onnx", 40);
    EXPECT_EQ(g.line(), "c.onnx · 3/6 · 40%");
    EXPECT_EQ(g.percent(), 40);
}

TEST(DownloadGroupProgress, IndexNeverExceedsTheTotal) {
    DownloadGroupProgress g;
    g.start(2);
    g.update("b", 90);
    g.fileFinished();
    g.fileFinished();
    g.fileFinished();   // 多报也不越界
    g.update("b", 100);
    EXPECT_EQ(g.line(), "b · 2/2 · 100%");
}

TEST(DownloadGroupProgress, StartingAgainResetsEverything) {
    DownloadGroupProgress g;
    g.start(3);
    g.update("a", 80);
    g.fileFinished();
    g.start(2);
    EXPECT_EQ(g.percent(), 0);
    EXPECT_TRUE(g.line().isEmpty());
    g.update("x", 5);
    EXPECT_EQ(g.line(), "x · 1/2 · 5%");
}

TEST(DownloadGroupProgress, PercentIsClamped) {
    DownloadGroupProgress g;
    g.start(1);
    g.update("a", 250);
    EXPECT_EQ(g.percent(), 100);
    g.update("a", -3);
    EXPECT_EQ(g.percent(), 0);
}
