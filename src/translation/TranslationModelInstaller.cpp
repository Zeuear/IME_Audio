#include "TranslationModelInstaller.h"
#include "TranslationModelStore.h"
#include "../utils/DownloadManager.h"
#include "../utils/Logger.h"

#include <QDir>
#include <QFile>
#include <QUrl>

TranslationModelInstaller::TranslationModelInstaller(QNetworkAccessManager* nam, const QString& modelsRoot,
                                                     QMap<QString, DirectionManifest> manifests, QObject* parent)
    : QObject(parent), m_modelsRoot(modelsRoot), m_manifests(std::move(manifests))
{
    m_downloads = new DownloadManager(nam, this);

    connect(m_downloads, &DownloadManager::groupStarted, this, &TranslationModelInstaller::installGroupStarted);
    connect(m_downloads, &DownloadManager::groupFileProgress, this,
            [this](const QString& groupId, const QString&, const QString& filename,
                   qint64 received, qint64 total, int overall) {
                emit installFileProgress(groupId, filename, received, total, overall);
            });
    connect(m_downloads, &DownloadManager::groupFileFinished, this,
            [this](const QString& groupId, const QString&, const QString& filename) {
                emit installFileFinished(groupId, filename);
            });
    connect(m_downloads, &DownloadManager::groupFileError, this,
            [this](const QString& groupId, const QString&, const QString& filename, const QString& error) {
                emit installFileError(groupId, filename, error);
            });
    connect(m_downloads, &DownloadManager::groupFinished, this, &TranslationModelInstaller::onGroupFinished);
}

QSet<QString> TranslationModelInstaller::knownPairs() const {
    const auto keys = m_manifests.keys();
    return QSet<QString>(keys.begin(), keys.end());
}

QString TranslationModelInstaller::groupIdFor(const QString& from, const QString& to) {
    return QStringLiteral("translation-%1-%2").arg(from, to);
}

bool TranslationModelInstaller::isInstalled(const QString& from, const QString& to) const {
    const auto it = m_manifests.constFind(from + QLatin1Char('>') + to);
    return it != m_manifests.constEnd() && TranslationModelStore::isInstalled(m_modelsRoot, it.value());
}

bool TranslationModelInstaller::isInstalling(const QString& from, const QString& to) const {
    return m_installing.contains(groupIdFor(from, to));
}

bool TranslationModelInstaller::isRouteInstalled(const TranslationRoute& route) const {
    if (route.kind == RouteKind::Unsupported) return false;
    for (const TranslationHop& hop : route.hops) {
        if (!isInstalled(hop.from, hop.to)) return false;
    }
    return true;
}

qint64 TranslationModelInstaller::bytesToDownload(const TranslationRoute& route) const {
    qint64 total = 0;
    for (const TranslationHop& hop : route.hops) {
        const auto it = m_manifests.constFind(hop.from + QLatin1Char('>') + hop.to);
        if (it == m_manifests.constEnd() || isInstalled(hop.from, hop.to)) continue;
        total += TranslationModelStore::totalBytes(it.value());
    }
    return total;
}

void TranslationModelInstaller::install(const TranslationRoute& route) {
    for (const TranslationHop& hop : route.hops) {
        const auto it = m_manifests.constFind(hop.from + QLatin1Char('>') + hop.to);
        if (it == m_manifests.constEnd()) continue;
        if (isInstalled(hop.from, hop.to) || isInstalling(hop.from, hop.to)) continue;
        installDirection(it.value());
    }
}

void TranslationModelInstaller::installDirection(const DirectionManifest& manifest) {
    const QString groupId = groupIdFor(manifest.from, manifest.to);
    const QString dir = TranslationModelStore::directionDir(m_modelsRoot, manifest.from, manifest.to);
    QDir().mkpath(dir);

    LOG_INFO(QString("Downloading translation model: %1 -> %2").arg(manifest.from, manifest.to));
    m_installing.insert(groupId);

    const QString displayName = tr("Translation model %1 → %2").arg(manifest.from, manifest.to);
    for (const ModelFile& f : manifest.files) {
        const QString savePath = TranslationModelStore::partPath(QDir(dir).absoluteFilePath(f.name));
        m_downloads->addGroupTask(groupId, displayName, QUrl(f.url), savePath);
    }
}

void TranslationModelInstaller::discardPartial(const DirectionManifest& manifest) const {
    const QDir dir(TranslationModelStore::directionDir(m_modelsRoot, manifest.from, manifest.to));
    for (const ModelFile& f : manifest.files) {
        QFile::remove(TranslationModelStore::partPath(dir.absoluteFilePath(f.name)));
    }
}

void TranslationModelInstaller::onGroupFinished(const QString& groupId, bool success) {
    if (!m_installing.remove(groupId)) return;

    for (const DirectionManifest& manifest : std::as_const(m_manifests)) {
        if (groupIdFor(manifest.from, manifest.to) != groupId) continue;

        QString error;
        if (!success) {
            discardPartial(manifest);
            error = QStringLiteral("模型下载失败，请检查网络后重试");
        }
        else if (!TranslationModelStore::finalizeDownload(m_modelsRoot, manifest, &error)) {
            success = false;
        }

        emit installGroupFinished(groupId, success, error);
        if (success) {
            LOG_INFO(QString("Translation model installed: %1 -> %2").arg(manifest.from, manifest.to));
            emit directionInstalled(manifest.from, manifest.to);
        }
        else {
            LOG_WARN(QString("Translation model install failed: %1 -> %2: %3").arg(manifest.from, manifest.to, error));
            emit installFailed(manifest.from, manifest.to, error);
        }
        return;
    }
}

QString TranslationModelInstaller::writeConfigFile(const QString& from, const QString& to) const {
    const auto it = m_manifests.constFind(from + QLatin1Char('>') + to);
    if (it == m_manifests.constEnd() || !isInstalled(from, to)) return {};

    const QString path = QDir(TranslationModelStore::directionDir(m_modelsRoot, from, to)).absoluteFilePath("config.yml");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return {};
    file.write(TranslationModelStore::generateBergamotYaml(m_modelsRoot, it.value()).toUtf8());
    return path;
}
