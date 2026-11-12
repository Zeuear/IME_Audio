#pragma once
#include <QObject>
#include <QString>

class Downloader;
class QNetworkAccessManager;

// @brief
// 人声分离模型（UVR-MDX-NET-Voc_FT，约 67 MB）的按需下载：第一次勾选"分离人声"时才下载，
// 不随安装包分发。下载后用固定的 SHA-256 校验，不符就丢弃，不会加载被篡改的模型。
class SeparationModelManager : public QObject {
    Q_OBJECT
public:
    explicit SeparationModelManager(QNetworkAccessManager* nam, QObject* parent = nullptr);

    static QString modelPath();
    static bool isReady();

    // 模型已就绪则立即发 ready()；正在下载时重复调用无效果
    void ensure();
    void cancel();
    bool isDownloading() const { return m_downloader != nullptr; }

signals:
    void progress(int percent);
    void ready();
    void failed(const QString& error);

private:
    QNetworkAccessManager* m_nam = nullptr;
    Downloader* m_downloader = nullptr;
};
