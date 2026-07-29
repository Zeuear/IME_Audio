#pragma once
#include <QObject>
#include <QProcess>
#include <QDir>
#include <QSettings>
#include <QCoreApplication>
#include <QDebug>
#include <QStandardPaths>
#include "../utils/Downloader.h"


struct GpuDetectionResult {
    bool hasNvidiaGpu = false;
    QString gpuName;
    bool hasCudaRuntime = false;
    QString cudaRuntimeVersion;
    bool hasCudnn = false;
    bool isFullyReady = false;
    QString failReason;
};


class CudaInstaller : public QObject
{
    Q_OBJECT
public:
    explicit CudaInstaller(QNetworkAccessManager* nam, QObject* parent = nullptr);
    ~CudaInstaller();

    static GpuDetectionResult detectGpuEnvironment(bool requireCudnn);
    static bool detectNvidiaGpuPresent(QString* gpuNameOut = nullptr);

    void startInstallCuda();
    void cancelInstallCuda();

signals:
    // 用于 DownloadListWidget 显示
    void installGroupStarted(const QString& groupId, const QString& displayName, int totalFiles);
    void installFileProgress(const QString& groupId, const QString& filename,  qint64 received, qint64 total, int overallPercent);
    void installFileFinished(const QString& groupId, const QString& filename);
    void installFileError(const QString& groupId, const QString& filename, const QString& error);
    void installGroupFinished(const QString& groupId, bool success, const QString& message);

    // 旧的信号（兼容原有代码）
    void statusChanged(const QString& status);
    void progressUpdated(int percent);
    void installStarted();
    void installFinished(bool success, const QString& message);

private slots:
    void onCudaDownloadProgress(const QString& fileName, qint64 received, qint64 total, double speed);
    void onCudnnDownloadProgress(const QString& fileName, qint64 received, qint64 total, double speed);

    void onCudaDownloadFinished(const QString& savePath);
    void onCudnnDownloadFinished(const QString& savePath);

    void onCudaDownloadError(const QString& errorString);
    void onCudnnDownloadError(const QString& errorString);

    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void startCudaInstallation();
    void startCudnnInstallation();
    void finishInstallation(bool success, const QString& message);

    void checkDownloadComplete();

    Downloader* m_cudaDownloader = nullptr;
    Downloader* m_cudnnDownloader = nullptr;
    QProcess* m_installProcess = nullptr;

    QString m_cudaInstallerPath;
    QString m_cudnnZipPath;

    bool m_cudaDownloaded = false;
    bool m_cudnnDownloaded = false;
    bool m_isInstalling = false;

    const QString GROUP_ID = "cuda_install";
};
