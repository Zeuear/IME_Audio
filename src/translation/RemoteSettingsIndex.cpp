#include "RemoteSettingsIndex.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

namespace {

bool isAllowedFileType(const QString& type) {
    return type == QStringLiteral("model") || type == QStringLiteral("lex") ||
           type == QStringLiteral("vocab") || type == QStringLiteral("srcvocab") ||
           type == QStringLiteral("trgvocab");
}

// 版本号形如 "1.10"、"1.9"，按数字分段比较；非数字段按 0 处理
int versionCompare(const QString& a, const QString& b) {
    const auto parts = [](const QString& v) {
        QList<int> nums;
        for (const QString& part : v.split(QLatin1Char('.'))) nums.append(part.toInt());
        return nums;
    };

    const QList<int> pa = parts(a);
    const QList<int> pb = parts(b);
    const int n = std::max(pa.size(), pb.size());
    for (int i = 0; i < n; ++i) {
        const int va = i < pa.size() ? pa[i] : 0;
        const int vb = i < pb.size() ? pb[i] : 0;
        if (va != vb) return va < vb ? -1 : 1;
    }
    return 0;
}

QString resolveAttachmentUrl(const QString& location, const QString& baseUrl) {
    if (location.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive) ||
        location.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
        return location;
    }

    QString url = baseUrl;
    if (!url.isEmpty() && !url.endsWith(QLatin1Char('/'))) {
        url.append(QLatin1Char('/'));
    }
    QString rel = location;
    while (rel.startsWith(QLatin1Char('/'))) {
        rel = rel.mid(1);
    }
    return url + rel;
}

qint64 attachmentSize(const QJsonObject& attachment) {
    if (!attachment.contains(QStringLiteral("size"))) return -1;
    bool ok = false;
    const qint64 size = attachment.value(QStringLiteral("size")).toVariant().toLongLong(&ok);
    return ok ? size : -1;
}

// 真实索引里约三分之二的记录没有顶层 hash，只有 attachment.hash，所以以后者为准
QString recordHash(const QJsonObject& record) {
    const QString hash = record.value(QStringLiteral("attachment")).toObject()
                             .value(QStringLiteral("hash")).toString();
    return hash.isEmpty() ? record.value(QStringLiteral("hash")).toString() : hash;
}

bool isCompleteRecord(const QJsonObject& record) {
    const QString from = record.value(QStringLiteral("fromLang")).toString();
    const QString to = record.value(QStringLiteral("toLang")).toString();
    const QString version = record.value(QStringLiteral("version")).toString();
    const QString fileType = record.value(QStringLiteral("fileType")).toString();
    const QString name = record.value(QStringLiteral("name")).toString();
    const QString hash = recordHash(record);
    const QJsonObject attachment = record.value(QStringLiteral("attachment")).toObject();

    if (from.isEmpty() || to.isEmpty() || version.isEmpty() || !isAllowedFileType(fileType) ||
        name.isEmpty() || hash.isEmpty() || !attachment.contains(QStringLiteral("location")) ||
        attachment.value(QStringLiteral("location")).toString().isEmpty() ||
        attachmentSize(attachment) < 0) {
        return false;
    }
    return true;
}

ModelFile parseModelFile(const QJsonObject& record, const QString& baseUrl) {
    ModelFile file;
    file.fileType = record.value(QStringLiteral("fileType")).toString();
    file.name = record.value(QStringLiteral("name")).toString();
    file.sha256 = recordHash(record);
    const QJsonObject attachment = record.value(QStringLiteral("attachment")).toObject();
    file.size = attachmentSize(attachment);
    file.url = resolveAttachmentUrl(attachment.value(QStringLiteral("location")).toString(), baseUrl);
    return file;
}

} // namespace

QMap<QString, DirectionManifest> parseRemoteSettingsIndex(const QByteArray& json,
                                                          const QString& attachmentBaseUrl) {
    QMap<QString, DirectionManifest> manifests;

    const QJsonDocument doc = QJsonDocument::fromJson(json);
    QJsonArray records;
    if (doc.isArray()) {
        records = doc.array();
    } else if (doc.isObject()) {
        const QJsonValue data = doc.object().value(QStringLiteral("data"));
        if (data.isArray()) records = data.toArray();
    }

    for (const QJsonValue& value : records) {
        if (!value.isObject()) continue;
        const QJsonObject record = value.toObject();
        if (!isCompleteRecord(record)) continue;

        const QString from = record.value(QStringLiteral("fromLang")).toString();
        const QString to = record.value(QStringLiteral("toLang")).toString();
        const QString version = record.value(QStringLiteral("version")).toString();
        const QString key = from + QStringLiteral(">") + to;

        ModelFile file = parseModelFile(record, attachmentBaseUrl);

        auto it = manifests.find(key);
        if (it == manifests.end() || versionCompare(version, it.value().version) > 0) {
            DirectionManifest manifest;
            manifest.from = from;
            manifest.to = to;
            manifest.version = version;
            manifest.files.append(file);
            manifests.insert(key, manifest);
        } else if (versionCompare(version, it.value().version) == 0) {
            it.value().files.append(file);
        }
        // 旧版本记录直接忽略，避免把不同版本的文件混在同一方向里
    }

    return manifests;
}
