// 文件转录任务列表：译文按 runToken 回填的前提，以及清缓存后链接任务的状态
#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QThread>

#include "ui/models/FileTaskModel.h"

namespace {
SubtitleCue cueOf(const QString& text) { return {0, 1000, text}; }
}

// 多个链接同时下载：先完成的会启动批次（reset 发新令牌），之后才下完的链接在转录中被并入队尾，
// 它们不经过 reset。令牌为 0 时 setCueTranslation 直接丢弃，这些视频就没有译文
TEST(FileTaskModelToken, LinkRowsHaveDistinctNonZeroTokensBeforeAnyReset) {
    FileTaskModel model;
    ASSERT_EQ(model.addLink(QStringLiteral("https://example.com/a")), 0);
    ASSERT_EQ(model.addLink(QStringLiteral("https://example.com/b")), 1);
    EXPECT_NE(model.task(0).runToken, 0u);
    EXPECT_NE(model.task(1).runToken, 0u);
    EXPECT_NE(model.task(0).runToken, model.task(1).runToken);
}

TEST(FileTaskModelToken, TranslationReachesRowAppendedWithoutReset) {
    FileTaskModel model;
    model.addLink(QStringLiteral("https://example.com/a"));
    model.addLink(QStringLiteral("https://example.com/b"));
    // 第二个链接没有 reset，直接转录
    model.appendCue(1, cueOf(QStringLiteral("hola")));
    model.setCueTranslation(model.task(1).runToken, 0, QStringLiteral("zh"), QStringLiteral("你好"));
    EXPECT_EQ(model.task(1).translations.value(0), QStringLiteral("你好"));
    EXPECT_TRUE(model.task(1).hasTranslation());
    EXPECT_FALSE(model.task(0).hasTranslation());
}

TEST(FileTaskModelToken, AddedFilesGetTokensAndResetRenewsThem) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString f1 = dir.filePath(QStringLiteral("a.wav"));
    const QString f2 = dir.filePath(QStringLiteral("b.wav"));
    for (const QString& p : {f1, f2}) {
        QFile f(p);
        ASSERT_TRUE(f.open(QIODevice::WriteOnly));
        f.write("x");
    }
    FileTaskModel model;
    ASSERT_EQ(model.addPaths({f1, f2}), 2);
    EXPECT_NE(model.task(0).runToken, 0u);
    EXPECT_NE(model.task(0).runToken, model.task(1).runToken);

    const quint64 old = model.task(0).runToken;
    model.reset(0);
    EXPECT_NE(model.task(0).runToken, old);   // 重转后，上一轮迟到的译文要被丢弃
}

TEST(FileTaskModelCache, DropCachedFilesClearsLinkPathAndVocalsOnly) {
    QTemporaryDir root;
    ASSERT_TRUE(root.isValid());
    const QString cache = root.filePath(QStringLiteral("link_downloads"));
    QDir().mkpath(cache + QStringLiteral("/1"));
    const QString linked = cache + QStringLiteral("/1/a.m4a");
    const QString local = root.filePath(QStringLiteral("mine.wav"));
    for (const QString& p : {linked, local}) {
        QFile f(p);
        ASSERT_TRUE(f.open(QIODevice::WriteOnly));
        f.write("x");
    }

    FileTaskModel model;
    model.addLink(QStringLiteral("https://example.com/a"));
    model.setDownloaded(0, linked, QStringLiteral("clip"));
    model.addPaths({local});
    model.setVocalsPath(1, root.filePath(QStringLiteral("v.wav")));
    model.appendText(0, QStringLiteral("hello"));

    EXPECT_EQ(model.dropCachedFiles(cache), 2);
    EXPECT_TRUE(model.task(0).path.isEmpty());       // 链接：之后再转录会重新下载
    EXPECT_EQ(model.task(0).text, QStringLiteral("hello"));   // 已有文字保留
    EXPECT_EQ(model.task(1).path, QFileInfo(local).absoluteFilePath());   // 用户自己的文件不动
    EXPECT_TRUE(model.task(1).vocalsPath.isEmpty());
}

TEST(FileTaskModelTiming, FormatsDurationsAndEstimatesRemaining) {
    EXPECT_EQ(FileTaskModel::formatDuration(0), QStringLiteral("0:00"));
    EXPECT_EQ(FileTaskModel::formatDuration(65'000), QStringLiteral("1:05"));
    EXPECT_EQ(FileTaskModel::formatDuration(3'725'000), QStringLiteral("1:02:05"));
    EXPECT_EQ(FileTaskModel::estimateRemainingMs(10'000, 0, 10), -1);        // 还没有完成的段：不估计
    EXPECT_EQ(FileTaskModel::estimateRemainingMs(10'000, 2, 10), 40'000);    // 2 段用了 10s，剩 8 段
    EXPECT_EQ(FileTaskModel::estimateRemainingMs(10'000, 10, 10), 0);
}

TEST(FileTaskModelTiming, TimerRunsWhileActiveFreezesOnFinishAndClearsOnReset) {
    QTemporaryDir dir;   // addPaths 只收真实存在的文件
    ASSERT_TRUE(dir.isValid());
    const QString wav = dir.filePath(QStringLiteral("a.wav"));
    QFile f(wav);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write("x");
    f.close();

    FileTaskModel model;
    ASSERT_EQ(model.addPaths({wav}), 1);
    EXPECT_TRUE(model.index(0).data(FileTaskModel::TimingTextRole).toString().isEmpty());   // 没开始：不显示

    model.markStarted(0);
    model.setProgress(0, 1, 4);
    EXPECT_GT(model.task(0).transcribeStartMs, 0);
    EXPECT_FALSE(model.index(0).data(FileTaskModel::TimingTextRole).toString().isEmpty());

    QThread::msleep(5);   // 耗时 > 0 才显示
    model.setState(0, FileTask::State::Done);
    EXPECT_EQ(model.task(0).startMs, 0);   // 定格，不再继续走
    EXPECT_FALSE(model.index(0).data(FileTaskModel::TimingTextRole).toString().isEmpty());

    model.reset(0);
    EXPECT_EQ(model.task(0).elapsedMs, 0);
    EXPECT_TRUE(model.index(0).data(FileTaskModel::TimingTextRole).toString().isEmpty());
}
