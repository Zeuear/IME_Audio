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

