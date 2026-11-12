#include <gtest/gtest.h>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "services/docs/DocsExtension.h"

namespace {
QByteArray readAll(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}
}

// 导出的文件夹必须能被 Chrome 直接加载：manifest 引用到的文件都在
TEST(DocsExtension, ExportedFolderHasEverythingTheManifestRefersTo) {
    QTemporaryDir tmp;
    const QString dir = tmp.filePath("ext");
    QString error;
    ASSERT_TRUE(DocsExtension::exportTo(dir, "tok", &error)) << error.toStdString();

    const QJsonObject manifest = QJsonDocument::fromJson(readAll(QDir(dir).filePath("manifest.json"))).object();
    ASSERT_FALSE(manifest.isEmpty());
    const QString worker = manifest.value("background").toObject().value("service_worker").toString();
    const QString popup = manifest.value("action").toObject().value("default_popup").toString();
    ASSERT_FALSE(worker.isEmpty());
    ASSERT_FALSE(popup.isEmpty());
    EXPECT_TRUE(QFile::exists(QDir(dir).filePath(worker)));
    EXPECT_TRUE(QFile::exists(QDir(dir).filePath(popup)));
    // 弹窗页面引用的脚本
    EXPECT_TRUE(QFile::exists(QDir(dir).filePath("popup.js")));
}

// 程序升级后再点一次菜单就是原地更新，旧文件必须能被覆盖
TEST(DocsExtension, ExportingAgainOverwritesOldFiles) {
    QTemporaryDir tmp;
    const QString dir = tmp.filePath("ext");
    ASSERT_TRUE(DocsExtension::exportTo(dir, "tok", nullptr));

    const QString manifestPath = QDir(dir).filePath("manifest.json");
    const QByteArray original = readAll(manifestPath);
    {
        QFile f(manifestPath);
        ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write("stale");
    }
    ASSERT_TRUE(DocsExtension::exportTo(dir, "tok", nullptr));
    EXPECT_EQ(readAll(manifestPath), original);
}

// 扩展连接时从 pairing.json 读令牌；重新导出要换成新令牌，旧的作废
TEST(DocsExtension, ExportWritesCurrentTokenForTheExtension) {
    QTemporaryDir tmp;
    const QString dir = tmp.filePath("ext");
    ASSERT_TRUE(DocsExtension::exportTo(dir, "first", nullptr));
    ASSERT_TRUE(DocsExtension::exportTo(dir, "second", nullptr));
    EXPECT_EQ(DocsExtension::readToken(dir), "second");
}
