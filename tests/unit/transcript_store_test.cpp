#include <gtest/gtest.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "ui/models/TranscriptStore.h"

TEST(TranscriptStore, SavedTextCanBeLoadedByANewInstance) {
    QTemporaryDir dir;
    const QString path = dir.filePath("session.txt");
    {
        TranscriptStore store(path);
        ASSERT_TRUE(store.save("你好，世界。\nHello."));
    }
    // 程序崩溃重启后，用新对象读回
    TranscriptStore reopened(path);
    EXPECT_EQ(reopened.load(), "你好，世界。\nHello.");
}

TEST(TranscriptStore, LoadingAMissingFileGivesEmptyText) {
    QTemporaryDir dir;
    TranscriptStore store(dir.filePath("none.txt"));
    EXPECT_TRUE(store.load().isEmpty());
}

TEST(TranscriptStore, SavingAShorterTextLeavesNoOldTail) {
    QTemporaryDir dir;
    TranscriptStore store(dir.filePath("session.txt"));
    ASSERT_TRUE(store.save("a very long first version of the transcript"));
    ASSERT_TRUE(store.save("short"));
    EXPECT_EQ(store.load(), "short");
}

TEST(TranscriptStore, MissingParentDirectoryIsCreated) {
    QTemporaryDir dir;
    TranscriptStore store(dir.filePath("transcripts/nested/session.txt"));
    ASSERT_TRUE(store.save("x"));
    EXPECT_EQ(store.load(), "x");
}

TEST(TranscriptStore, FailedSaveReportsFalseAndKeepsOtherFilesUntouched) {
    QTemporaryDir dir;
    // 父路径是一个普通文件，目录建不出来
    QFile blocker(dir.filePath("blocker"));
    ASSERT_TRUE(blocker.open(QIODevice::WriteOnly));
    blocker.write("keep");
    blocker.close();

    TranscriptStore store(dir.filePath("blocker/session.txt"));
    EXPECT_FALSE(store.save("x"));

    QFile check(dir.filePath("blocker"));
    ASSERT_TRUE(check.open(QIODevice::ReadOnly));
    EXPECT_EQ(check.readAll(), "keep");
}

TEST(TranscriptStore, RemoveDeletesTheSessionFile) {
    QTemporaryDir dir;
    const QString path = dir.filePath("session.txt");
    TranscriptStore store(path);
    ASSERT_TRUE(store.save("secret"));
    store.remove();
    EXPECT_FALSE(QFileInfo::exists(path));
    EXPECT_TRUE(store.load().isEmpty());
    store.remove();   // 文件不存在时再删也不出错
}

TEST(TranscriptStore, SaveLeavesNoStrayTemporaryFilesBehind) {
    QTemporaryDir dir;
    TranscriptStore store(dir.filePath("session.txt"));
    ASSERT_TRUE(store.save("one"));
    ASSERT_TRUE(store.save("two"));
    EXPECT_EQ(QDir(dir.path()).entryList(QDir::Files).size(), 1);
}

TEST(TranscriptStore, ExportWritesPlainUtf8Text) {
    QTemporaryDir dir;
    const QString out = dir.filePath("export.txt");
    ASSERT_TRUE(TranscriptStore::exportTxt(out, "第一句。\n第二句。"));
    QFile f(out);
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    // 独立的预期值：UTF-8 字节，不带 BOM
    EXPECT_EQ(f.readAll(), QByteArray::fromStdString("第一句。\n第二句。"));
}

TEST(TranscriptStore, ExportToUnwritableLocationReportsFalse) {
    QTemporaryDir dir;
    EXPECT_FALSE(TranscriptStore::exportTxt(dir.filePath("missing_dir/export.txt"), "x"));
}
