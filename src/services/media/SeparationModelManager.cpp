#include "SeparationModelManager.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "utils/AppPaths.h"
#include "utils/Downloader.h"
#include "utils/Logger.h"

namespace {

constexpr char kModelUrl[] =
    "https://github.com/TRvlvr/model_repo/releases/download/all_public_uvr_models/UVR-MDX-NET-Voc_FT.onnx";
constexpr char kModelSha256[] = "534b2070fcc7df514b13ef660dc8cbb328679c2374d04354a5c42bb14ecce111";
constexpr qint64 kModelSize = 66762490;
constexpr int kStallTimeoutMs = 30000;

QString sha256OfFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&f)) return {};
    return QString::fromLatin1(hash.result().toHex());
}

}

SeparationModelManager::SeparationModelManager(QNetworkAccessManager* nam, QObject* parent)
    : QObject(parent), m_nam(nam)
{
}

QString SeparationModelManager::modelPath()
{
    return QDir(AppPaths::toolsDir()).absoluteFilePath(QStringLiteral("uvr/UVR-MDX-NET-Voc_FT.onnx"));
}

bool SeparationModelManager::isReady()
{
    // 文件只会在校验通过后才改成正式名，所以大小对得上即可，不必每次重算哈希
    return QFileInfo(modelPath()).size() == kModelSize;
}

void SeparationModelManager::ensure()
{
    if (isReady()) {
        emit ready();
        return;
    }
    if (m_downloader) return;

    QDir().mkpath(QFileInfo(modelPath()).absolutePath());
    auto* dl = new Downloader(m_nam, this);
    m_downloader = dl;
    dl->setUserAgent("ImeAudio");
    dl->setStallTimeoutMs(kStallTimeoutMs);
    dl->setVerifier([](const QString& partPath) {
        return sha256OfFile(partPath) == QLatin1String(kModelSha256)
            ? QString() : QStringLiteral("人声分离模型校验失败，已放弃使用");
    });

    connect(dl, &Downloader::progress, this, [this](const QString&, qint64 received, qint64 total, double) {
        if (total > 0) emit progress(int(received * 100 / total));
    });
    connect(dl, &Downloader::finished, this, [this, dl]() {
        dl->deleteLater();
        if (m_downloader == dl) m_downloader = nullptr;
        LOG_INFO("SeparationModelManager: vocal model downloaded");
        emit ready();
    });
    connect(dl, &Downloader::error, this, [this, dl](const QString& err) {
        dl->deleteLater();
        if (m_downloader == dl) m_downloader = nullptr;
        LOG_WARN(QString("SeparationModelManager: download failed: %1").arg(err));
        emit failed(QStringLiteral("人声分离模型下载失败：") + err);
    });
    dl->start(QUrl(QString::fromLatin1(kModelUrl)), modelPath());
}

void SeparationModelManager::cancel()
{
    if (!m_downloader) return;
    Downloader* dl = m_downloader;
    m_downloader = nullptr;
    dl->disconnect(this);
    dl->cancel();
    dl->deleteLater();
}
