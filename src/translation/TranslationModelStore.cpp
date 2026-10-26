#include "TranslationModelStore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace TranslationModelStore {

QMap<QString, DirectionManifest> parseBundledManifest(const QByteArray& json) {
    QMap<QString, DirectionManifest> result;

    const QJsonObject root = QJsonDocument::fromJson(json).object();
    const QString baseUrl = root.value(QStringLiteral("baseUrl")).toString();
    const QJsonObject directions = root.value(QStringLiteral("directions")).toObject();

    for (auto it = directions.begin(); it != directions.end(); ++it) {
        const QStringList langs = it.key().split(QLatin1Char('>'));
        if (langs.size() != 2) continue;

        const QJsonObject entry = it.value().toObject();
        DirectionManifest manifest;
        manifest.from = langs[0];
        manifest.to = langs[1];
        manifest.version = entry.value(QStringLiteral("version")).toString();

        for (const QJsonValue& v : entry.value(QStringLiteral("files")).toArray()) {
            const QJsonObject f = v.toObject();
            ModelFile file;
            file.fileType = f.value(QStringLiteral("type")).toString();
            file.name = f.value(QStringLiteral("name")).toString();
            file.sha256 = f.value(QStringLiteral("sha256")).toString();
            file.size = f.value(QStringLiteral("size")).toVariant().toLongLong();
            file.url = baseUrl + f.value(QStringLiteral("location")).toString();
            if (file.name.isEmpty() || file.sha256.isEmpty()) continue;
            manifest.files.append(file);
        }
        if (!manifest.files.isEmpty()) result.insert(it.key(), manifest);
    }
    return result;
}

QString directionDir(const QString& modelsRoot, const QString& from, const QString& to) {
    return QDir(modelsRoot).absoluteFilePath(from + QLatin1Char('-') + to);
}

QString partPath(const QString& finalPath) {
    return finalPath + QStringLiteral(".part");
}

bool isInstalled(const QString& modelsRoot, const DirectionManifest& manifest) {
    if (manifest.files.isEmpty()) return false;

    const QDir dir(directionDir(modelsRoot, manifest.from, manifest.to));
    for (const ModelFile& f : manifest.files) {
        const QFileInfo info(dir.absoluteFilePath(f.name));
        if (!info.exists() || info.size() != f.size) return false;
    }
    return true;
}

qint64 totalBytes(const DirectionManifest& manifest) {
    qint64 total = 0;
    for (const ModelFile& f : manifest.files) total += f.size;
    return total;
}

bool verifySha256(const QString& path, const QString& expectedHex) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) return false;
    return hash.result().toHex().compare(expectedHex.toLatin1(), Qt::CaseInsensitive) == 0;
}

bool finalizeDownload(const QString& modelsRoot, const DirectionManifest& manifest, QString* error) {
    const QDir dir(directionDir(modelsRoot, manifest.from, manifest.to));

    const auto cleanup = [&]() {
        for (const ModelFile& f : manifest.files) QFile::remove(partPath(dir.absoluteFilePath(f.name)));
    };

    for (const ModelFile& f : manifest.files) {
        const QString finalPath = dir.absoluteFilePath(f.name);
        const QString part = partPath(finalPath);

        if (!verifySha256(part, f.sha256)) {
            cleanup();
            if (error) *error = QStringLiteral("模型文件校验失败：%1").arg(f.name);
            return false;
        }
        QFile::remove(finalPath);
        if (!QFile::rename(part, finalPath)) {
            cleanup();
            if (error) *error = QStringLiteral("无法保存模型文件：%1").arg(f.name);
            return false;
        }
    }
    return true;
}

QString generateBergamotYaml(const QString& modelsRoot, const DirectionManifest& manifest) {
    const QDir dir(directionDir(modelsRoot, manifest.from, manifest.to));
    const auto pathOf = [&](const QString& type) {
        for (const ModelFile& f : manifest.files) {
            if (f.fileType == type) return QDir::fromNativeSeparators(dir.absoluteFilePath(f.name));
        }
        return QString();
    };

    // 目标是中日韩的方向源、目标词表分开；其余方向两边共用同一个 vocab
    const QString shared = pathOf(QStringLiteral("vocab"));
    const QString srcVocab = shared.isEmpty() ? pathOf(QStringLiteral("srcvocab")) : shared;
    const QString trgVocab = shared.isEmpty() ? pathOf(QStringLiteral("trgvocab")) : shared;

    QString yaml;
    yaml += QStringLiteral("models: [%1]\n").arg(pathOf(QStringLiteral("model")));
    yaml += QStringLiteral("vocabs: [%1, %2]\n").arg(srcVocab, trgVocab);
    yaml += QStringLiteral("shortlist: [%1, false]\n").arg(pathOf(QStringLiteral("lex")));
    yaml += QStringLiteral(
        "beam-size: 1\n"
        "normalize: 1.0\n"
        "word-penalty: 0\n"
        "max-length-break: 128\n"
        "mini-batch-words: 1024\n"
        "workspace: 128\n"
        "max-length-factor: 2.0\n"
        "skip-cost: true\n"
        "cpu-threads: 0\n"
        "quiet: true\n"
        "quiet-translation: true\n"
        "gemm-precision: int8shiftAlphaAll\n"
        "alignment: soft\n"
        "ssplit-mode: paragraph\n");
    return yaml;
}

}
