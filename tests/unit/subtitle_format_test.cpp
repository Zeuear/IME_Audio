#include <gtest/gtest.h>

#include "widgets/SubtitleFormat.h"

TEST(SubtitleFormat, FormatsTimestamp) {
    EXPECT_EQ(formatSrtTime(0), QString("00:00:00,000"));
    EXPECT_EQ(formatSrtTime(1234), QString("00:00:01,234"));
    EXPECT_EQ(formatSrtTime(3723004), QString("01:02:03,004"));
}

TEST(SubtitleFormat, TimestampDoesNotTruncateLongHours) {
    EXPECT_EQ(formatSrtTime(100LL * 3600000), QString("100:00:00,000"));
}

TEST(SubtitleFormat, NegativeTimeClampsToZero) {
    EXPECT_EQ(formatSrtTime(-5), QString("00:00:00,000"));
}

TEST(SubtitleFormat, BuildsNumberedEntriesSeparatedByBlankLine) {
    const QList<SubtitleCue> cues = {{0, 1500, "你好"}, {2000, 4200, "world"}};
    EXPECT_EQ(buildSrt(cues),
              QString("1\n00:00:00,000 --> 00:00:01,500\n你好\n\n"
                      "2\n00:00:02,000 --> 00:00:04,200\nworld\n\n"));
}

TEST(SubtitleFormat, SkipsEmptyCuesAndKeepsNumberingContiguous) {
    const QList<SubtitleCue> cues = {{0, 1000, "a"}, {1000, 2000, "   "}, {2000, 3000, "b"}};
    const QString srt = buildSrt(cues);
    EXPECT_TRUE(srt.contains("2\n00:00:02,000 --> 00:00:03,000\nb"));
    EXPECT_FALSE(srt.contains("3\n"));
}

TEST(SubtitleFormat, RemovesBlankLinesInsideText) {
    const QList<SubtitleCue> cues = {{0, 1000, "first\n\nsecond"}};
    EXPECT_EQ(buildSrt(cues), QString("1\n00:00:00,000 --> 00:00:01,000\nfirst\nsecond\n\n"));
}

TEST(SubtitleFormat, EndNeverEarlierThanStart) {
    const QList<SubtitleCue> cues = {{5000, 5000, "x"}, {6000, 4000, "y"}};
    const QString srt = buildSrt(cues);
    EXPECT_TRUE(srt.contains("00:00:05,000 --> 00:00:05,001"));
    EXPECT_TRUE(srt.contains("00:00:06,000 --> 00:00:06,001"));
}

TEST(SubtitleFormat, EmptyInputGivesEmptyOutput) {
    EXPECT_TRUE(buildSrt({}).isEmpty());
}
