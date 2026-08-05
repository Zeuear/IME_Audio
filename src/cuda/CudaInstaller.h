#pragma once
#include <QObject>
#include <QProcess>
#include <QDir>
#include <QSettings>
#include <QCoreApplication>
#include <QDebug>
#include <QStandardPaths>
#include "../utils/DownloadManager.h"
#include "../utils/ProcessManager.h"
#include "../utils/ExtractTool.h"
#include "../sherpa/SherpaManager.h"

#ifdef Q_OS_WIN32
#include <Windows.h>
#include <shellapi.h>
#endif


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
    explicit CudaInstaller(QNetworkAccessManager* nam, SherpaInstaller* sherpaInstaller, QObject* parent = nullptr);
    ~CudaInstaller();

    void setEnvironment();
    static GpuDetectionResult detectGpuEnvironment(bool requireCudnn);
    static bool detectNvidiaGpuPresent(QString* gpuNameOut = nullptr);

    void startDownload(const GpuDetectionResult& result);
    void cancelDownload();
    void startInstall();

signals:
    // 用于 DownloadListWidget 显示
    void installGroupStarted(const QString& groupId, const QString& displayName, int totalFiles);
    void installGroupFinished(const QString& groupId, bool success, const QString& message);
    void installFileProgress(const QString& groupId, const QString& filename,  qint64 received, qint64 total, int overallPercent);
    void installFileFinished(const QString& groupId, const QString& filename);
    void installFileError(const QString& groupId, const QString& filename, const QString& error);

    // 解压进度（仅真正走解压的任务才发）
    void extractStarted(const QString& groupId, int totalLines);
    void extractProgress(const QString& groupId, int current, int total);
    void extractFinished(const QString& groupId, bool success);

    void statusChanged(const QString& status);
    void progressUpdated(int percent);
    void installStarted();
    void installFinished(bool success, const QString& message);

private slots:
    void onGroupFileProgress(const QString& groupId, const QString& taskId, const QString& filename, qint64 received, qint64 total, int overallPercent);
    void onGroupFileFinished(const QString& groupId, const QString& taskId, const QString& filename);
    void onGroupFileError(const QString& groupId, const QString& taskId, const QString& filename, const QString& error);
    void onGroupFinished(const QString& groupId, bool success);

private:
    void finishInstallation(bool success, const QString& message);
    
    GpuDetectionResult m_detail;
    DownloadManager* m_cudaDownloader = nullptr;
    TaskQueueManager* m_taskManager = nullptr;
    SherpaInstaller* m_sherpaInstaller = nullptr;

    QString m_cudaInstallerPath;
    QString m_cudnnZipPath;
    QString m_sherpaZipPath;

    const QString GROUP_ID = "cuda_install";
};
