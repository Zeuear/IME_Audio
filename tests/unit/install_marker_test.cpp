#include <gtest/gtest.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "sherpa/InstallMarker.h"

namespace {
void touch(const QString& path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write("x");
}
} // namespace

TEST(InstallMarker, MissingDirectoryIsNotInstalledAndNotIncomplete) {
    QTemporaryDir root;
    const QString dir = root.filePath("model-a");
    EXPECT_FALSE(InstallMarker::isInstalled(dir));
    EXPECT_FALSE(InstallMarker::isIncomplete(dir));
}

TEST(InstallMarker, OldInstallWithoutMarkerCountsAsInstalled) {
    // 升级前就装好的模型没有标记，不能因此被当成没装好而重新下载
    QTemporaryDir root;
    const QString dir = root.filePath("model-a");
    touch(dir + "/model.onnx");
    EXPECT_TRUE(InstallMarker::isInstalled(dir));
    EXPECT_FALSE(InstallMarker::isIncomplete(dir));
}

TEST(InstallMarker, MarkInstallingMakesTheDirectoryIncomplete) {
    QTemporaryDir root;
    const QString dir = root.filePath("model-a");
    ASSERT_TRUE(InstallMarker::markInstalling(dir));
    touch(dir + "/model.onnx");   // 已经下完的第一个文件
    EXPECT_FALSE(InstallMarker::isInstalled(dir));
    EXPECT_TRUE(InstallMarker::isIncomplete(dir));
}

TEST(InstallMarker, MarkCompleteMakesItInstalled) {
    QTemporaryDir root;
    const QString dir = root.filePath("model-a");
    ASSERT_TRUE(InstallMarker::markInstalling(dir));
    touch(dir + "/model.onnx");
    InstallMarker::markComplete(dir);
    EXPECT_TRUE(InstallMarker::isInstalled(dir));
    EXPECT_FALSE(InstallMarker::isIncomplete(dir));
    EXPECT_TRUE(QFileInfo::exists(dir + "/model.onnx"));   // 只删标记，不动模型文件
}

TEST(InstallMarker, RepeatedCallsAreHarmless) {
    QTemporaryDir root;
    const QString dir = root.filePath("model-a");
    EXPECT_TRUE(InstallMarker::markInstalling(dir));
    EXPECT_TRUE(InstallMarker::markInstalling(dir));
    InstallMarker::markComplete(dir);
    InstallMarker::markComplete(dir);   // 标记已经不在，再删也不出错
    EXPECT_TRUE(InstallMarker::isInstalled(dir));
}

TEST(InstallMarker, MarkInstallingCreatesMissingParentDirectories) {
    QTemporaryDir root;
    const QString dir = root.filePath("models/nested/model-a");
    ASSERT_TRUE(InstallMarker::markInstalling(dir));
    EXPECT_TRUE(QFileInfo(dir).isDir());
}

TEST(InstallMarker, MarkInstallingFailsWhenTheParentIsAFile) {
    QTemporaryDir root;
    touch(root.filePath("blocker"));
    EXPECT_FALSE(InstallMarker::markInstalling(root.filePath("blocker/model-a")));
}
