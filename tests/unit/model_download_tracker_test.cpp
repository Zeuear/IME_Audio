#include <gtest/gtest.h>
#include <QList>
#include <QString>
#include <QStringList>

#include "ui/models/ModelDownloadTracker.h"

using State = ModelDownloadTracker::State;

namespace {
const QString kA = "org/model-a";
const QString kB = "org/model-b";
}

TEST(ModelDownloadTracker, UnknownModelHasNoState) {
    ModelDownloadTracker t;
    EXPECT_EQ(t.info(kA).state, State::None);
}

TEST(ModelDownloadTracker, StartingShowsDownloadingAtZero) {
    ModelDownloadTracker t;
    t.onGroupStarted(kA, "Model A", 6);
    EXPECT_EQ(t.info(kA).state, State::Downloading);
    EXPECT_EQ(t.info(kA).percent, 0);
}

TEST(ModelDownloadTracker, ProgressShowsFileIndexAndPercent) {
    ModelDownloadTracker t;
    t.onGroupStarted(kA, "Model A", 6);
    t.onFileProgress(kA, "a.onnx", 10, 100, 40);
    EXPECT_EQ(t.info(kA).percent, 40);
    EXPECT_EQ(t.info(kA).text, "a.onnx · 1/6 · 40%");
    t.onFileFinished(kA, "a.onnx");
    t.onFileProgress(kA, "b.onnx", 1, 100, 55);
    EXPECT_EQ(t.info(kA).text, "b.onnx · 2/6 · 55%");
}

TEST(ModelDownloadTracker, ProgressWithoutAStartIsStillTracked) {
    // 信号连上之前就开始的下载：来了进度就当作下载中
    ModelDownloadTracker t;
    t.onFileProgress(kA, "a.onnx", 1, 100, 12);
    EXPECT_EQ(t.info(kA).state, State::Downloading);
    EXPECT_EQ(t.info(kA).percent, 12);
}

TEST(ModelDownloadTracker, ExtractingReportsItsOwnPercent) {
    ModelDownloadTracker t;
    t.onGroupStarted(kA, "Model A", 1);
    t.onFileProgress(kA, "a.tar.bz2", 100, 100, 100);
    t.onExtractStarted(kA, 12);
    EXPECT_EQ(t.info(kA).state, State::Extracting);
    EXPECT_EQ(t.info(kA).percent, 0);
    t.onExtractProgress(kA, 3, 12);
    EXPECT_EQ(t.info(kA).percent, 25);
    t.onExtractProgress(kA, 12, 12);
    EXPECT_EQ(t.info(kA).percent, 100);
}

TEST(ModelDownloadTracker, ExtractProgressWithUnknownTotalIsZero) {
    ModelDownloadTracker t;
    t.onExtractStarted(kA, 0);
    t.onExtractProgress(kA, 5, 0);
    EXPECT_EQ(t.info(kA).percent, 0);
}

TEST(ModelDownloadTracker, SuccessClearsTheState) {
    ModelDownloadTracker t;
    t.onGroupStarted(kA, "Model A", 2);
    t.onFileProgress(kA, "a", 1, 2, 50);
    t.onGroupFinished(kA, true, "ok");
    EXPECT_EQ(t.info(kA).state, State::None);
}

TEST(ModelDownloadTracker, FailureShowsTheFileErrorAsTheReason) {
    ModelDownloadTracker t;
    t.onGroupStarted(kA, "Model A", 2);
    t.onFileError(kA, "a.onnx", "Host not found");
    t.onGroupFinished(kA, false, "Download Error");
    EXPECT_EQ(t.info(kA).state, State::Failed);
    EXPECT_EQ(t.info(kA).text, "Host not found");
}

TEST(ModelDownloadTracker, FailureWithoutAFileErrorUsesTheGroupMessage) {
    ModelDownloadTracker t;
    t.onGroupStarted(kA, "Model A", 1);
    t.onGroupFinished(kA, false, "Extract Error");
    EXPECT_EQ(t.info(kA).state, State::Failed);
    EXPECT_EQ(t.info(kA).text, "Extract Error");
}

TEST(ModelDownloadTracker, CancellingIsNotAFailure) {
    ModelDownloadTracker t;
    t.onGroupStarted(kA, "Model A", 3);
    t.onFileProgress(kA, "a", 1, 2, 30);
    t.markCancelled(kA);
    t.onGroupFinished(kA, false, "Download Error");   // 取消会让下载层报告"组失败"，不能当成真的失败
    EXPECT_EQ(t.info(kA).state, State::None);
}

TEST(ModelDownloadTracker, RetryAfterFailureStartsCleanAndForgetsTheOldError) {
    ModelDownloadTracker t;
    t.onGroupStarted(kA, "Model A", 2);
    t.onFileError(kA, "a", "boom");
    t.onGroupFinished(kA, false, "Download Error");
    ASSERT_EQ(t.info(kA).state, State::Failed);

    t.onGroupStarted(kA, "Model A", 2);
    EXPECT_EQ(t.info(kA).state, State::Downloading);
    EXPECT_EQ(t.info(kA).percent, 0);
    EXPECT_TRUE(t.info(kA).text.isEmpty());
    // 再失败一次，不能带着上一次的错误原因
    t.onGroupFinished(kA, false, "Download Error");
    EXPECT_EQ(t.info(kA).text, "Download Error");
}

TEST(ModelDownloadTracker, CancelFlagDoesNotLeakIntoTheNextAttempt) {
    ModelDownloadTracker t;
    t.onGroupStarted(kA, "Model A", 1);
    t.markCancelled(kA);
    t.onGroupFinished(kA, false, "Download Error");
    t.onGroupStarted(kA, "Model A", 1);               // 用户重新下载
    t.onGroupFinished(kA, false, "Download Error");   // 这次是真的失败
    EXPECT_EQ(t.info(kA).state, State::Failed);
}

TEST(ModelDownloadTracker, ModelsDoNotInterfereWithEachOther) {
    ModelDownloadTracker t;
    t.onGroupStarted(kA, "Model A", 2);
    t.onGroupStarted(kB, "Model B", 3);
    t.onFileProgress(kA, "a", 1, 2, 70);
    t.onFileProgress(kB, "b", 1, 2, 20);
    t.onGroupFinished(kA, true, "ok");
    EXPECT_EQ(t.info(kA).state, State::None);
    EXPECT_EQ(t.info(kB).state, State::Downloading);
    EXPECT_EQ(t.info(kB).percent, 20);
}

TEST(ModelDownloadTracker, EveryChangeIsAnnouncedWithTheModelId) {
    ModelDownloadTracker t;
    QStringList changed;
    QObject::connect(&t, &ModelDownloadTracker::changed, &t, [&](const QString& id) { changed << id; });
    t.onGroupStarted(kA, "Model A", 2);
    t.onFileProgress(kA, "a", 1, 2, 10);
    t.onGroupFinished(kA, true, "ok");
    EXPECT_EQ(changed, (QStringList{ kA, kA, kA }));
}
