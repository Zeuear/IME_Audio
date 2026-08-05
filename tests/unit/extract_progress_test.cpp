#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QString>

#include "utils/ExtractTool.h"

// Helper: create N files in dir, then tar them into archivePath.
// Returns the number of regular files placed inside the archive.
static int makeTarArchive(const QString& dir, const QString& archivePath, int n)
{
    QDir d(dir);
    for (int i = 0; i < n; ++i) {
        QFile f(d.filePath(QString("file_%1.txt").arg(i)));
        EXPECT_TRUE(f.open(QIODevice::WriteOnly));
        f.write(QByteArray("hello ").append(QByteArray::number(i)));
        f.close();
    }
    QProcess tar;
    tar.setProgram("tar");
    tar.setArguments({ "cf", archivePath, "-C", dir, "." });
    tar.start();
    EXPECT_TRUE(tar.waitForFinished(30000));
    EXPECT_EQ(tar.exitCode(), 0);
    return n;
}

class ExtractProgressTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!qApp) {
            static int argc = 0;
            static char* argv[] = { nullptr };
            m_app = new QCoreApplication(argc, argv);
        }
    }
    QCoreApplication* m_app = nullptr;
};

// 不变量 (a): 解压回调单调递增，结束 current==total，且首回调 total>0
TEST_F(ExtractProgressTest, ProgressMonotonicAndReachesTotal) {
    QTemporaryDir srcDir;
    QTemporaryDir workDir;
    ASSERT_TRUE(srcDir.isValid() && workDir.isValid());

    const int kFiles = 7;
    int total = makeTarArchive(srcDir.path(), workDir.path() + "/a.tar", kFiles);
    ASSERT_GT(total, 0);

    QString dest = workDir.path() + "/out";
    QDir().mkpath(dest);

    int firstTotal = -1;
    int lastCurrent = -1;
    int lastTotal = -1;
    int prevCurrent = -1;
    bool sawProgress = false;

    bool ok = ExtractTool::extractAll(
        workDir.path() + "/a.tar", dest, false,
        [&](const ExtractProgress& p) {
            sawProgress = true;
            if (firstTotal < 0) firstTotal = p.total;
            EXPECT_GE(p.current, p.total == -1 ? 0 : 1);
            if (prevCurrent >= 0) EXPECT_GE(p.current, prevCurrent); // 单调不减
            prevCurrent = p.current;
            lastCurrent = p.current;
            lastTotal = p.total;
        });

    EXPECT_TRUE(ok);
    ASSERT_TRUE(sawProgress);
    EXPECT_EQ(firstTotal, total);
    EXPECT_EQ(lastTotal, total);
    EXPECT_EQ(lastCurrent, total);
}

// 不变量 (b): onProgress 为 nullptr 时解压仍成功、不崩溃
TEST_F(ExtractProgressTest, NullCallbackDoesNotCrash) {
    QTemporaryDir srcDir;
    QTemporaryDir workDir;
    ASSERT_TRUE(srcDir.isValid() && workDir.isValid());

    const int kFiles = 3;
    makeTarArchive(srcDir.path(), workDir.path() + "/b.tar", kFiles);

    QString dest = workDir.path() + "/out2";
    QDir().mkpath(dest);

    bool ok = ExtractTool::extractAll(workDir.path() + "/b.tar", dest, false, nullptr);
    EXPECT_TRUE(ok);
}
