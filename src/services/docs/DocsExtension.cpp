#include "DocsExtension.h"
#include "utils/AppPaths.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace DocsExtension {

namespace {
const QString kResourceRoot = QStringLiteral(":/docs-extension");
}

QString installDir()
{
    return QDir(AppPaths::dataDir()).filePath(QStringLiteral("extensions/google-docs"));
}

QString pairingFileName()
{
    return QStringLiteral("pairing.json");
}

bool exportTo(const QString& dir, const QString& token, QString* error)
{
    const QStringList names = QDir(kResourceRoot).entryList(QDir::Files);
    if (names.isEmpty()) {
        if (error) *error = QStringLiteral("程序里没有找到扩展文件");
        return false;
    }
    if (!QDir().mkpath(dir)) {
        if (error) *error = QStringLiteral("无法创建文件夹：%1").arg(dir);
        return false;
    }

    for (const QString& name : names) {
        QFile in(kResourceRoot + QLatin1Char('/') + name);
        // 不用 QFile::copy：从资源拷出来的文件是只读的，下次更新时覆盖不了
        QFile out(QDir(dir).filePath(name));
        if (!in.open(QIODevice::ReadOnly) || !out.open(QIODevice::WriteOnly | QIODevice::Truncate)
            || out.write(in.readAll()) < 0) {
            if (error) *error = QStringLiteral("无法写入 %1，请检查磁盘空间和目录权限").arg(out.fileName());
            return false;
        }
    }

    QFile pairing(QDir(dir).filePath(pairingFileName()));
    const QByteArray json = QJsonDocument(QJsonObject{{"token", token}}).toJson(QJsonDocument::Compact);
    if (!pairing.open(QIODevice::WriteOnly | QIODevice::Truncate) || pairing.write(json) < 0) {
        if (error) *error = QStringLiteral("无法写入 %1，请检查磁盘空间和目录权限").arg(pairing.fileName());
        return false;
    }
    return true;
}

QString readToken(const QString& dir)
{
    QFile f(QDir(dir).filePath(pairingFileName()));
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object().value("token").toString();
}

} // namespace DocsExtension
