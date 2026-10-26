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



struct CudaLibCandidate {
    QString libName;
    QString version;
};

struct CudaPlatformSpec {
    // 能否在本平台探测已安装的 CUDA 环境
    bool detectable = false;
    // 能否由本程序代为下载并安装 CUDA/cuDNN（仅 Windows 提供了安装包与静默参数）
    bool installable = false;

    QVector<CudaLibCandidate> cudartCandidates;
    QStringList cudnnCandidates;
    QString ortCudaProviderLib;
    QString nvidiaSmiExecutable;

    QString cudaInstallerFileName;
    QString cudaInstallerUrl;
    QStringList cudaInstallerArgs;

    QString cudnnArchiveFileName;
    QString cudnnArchiveUrl;

    QString sherpaArchiveFileName;
    QString sherpaArchiveUrl;
};

inline const CudaPlatformSpec& cudaPlatformSpec() {
    static const CudaPlatformSpec spec = [] {
        CudaPlatformSpec s;
#if defined(Q_OS_WIN)
        s.detectable = true;
        s.installable = true;
        s.cudartCandidates = { { "cudart64_12", "12.x" }, { "cudart64_13", "13.x" } };
        s.cudnnCandidates = { "cudnn64_9" };
        s.ortCudaProviderLib = "onnxruntime_providers_cuda.dll";
        s.nvidiaSmiExecutable = "nvidia-smi.exe";

        s.cudaInstallerFileName = "cuda_12.6.0_560.76_windows.exe";
        s.cudaInstallerUrl = "https://developer.download.nvidia.com/compute/cuda/12.6.0/"
                             "local_installers/cuda_12.6.0_560.76_windows.exe";
        s.cudaInstallerArgs = { "/s", "-n", "nvcc_12.6", "cusparse_12.6", "cublas_12.6", "cudart_12.6" };

        s.cudnnArchiveFileName = "cudnn-windows-x86_64-9.6.0.29_cuda12-archive.zip";
        s.cudnnArchiveUrl = "https://developer.download.nvidia.com/compute/cudnn/redist/cudnn/"
                            "windows-x86_64/cudnn-windows-x86_64-9.6.0.74_cuda12-archive.zip";

        s.sherpaArchiveFileName = "sherpa-onnx-v1.13.4.tar.bz2";
        s.sherpaArchiveUrl = "https://github.com/k2-fsa/sherpa-onnx/releases/download/v1.13.4/"
                             "sherpa-onnx-v1.13.4-cuda-12.x-cudnn-9.x-win-x64-cuda.tar.bz2";
#elif defined(Q_OS_LINUX)
        // Linux 能探测系统上已有的 CUDA，但本程序不提供 Linux 安装包，交由发行版包管理器安装。
        s.detectable = true;
        s.installable = false;
        s.cudartCandidates = { { "cudart", "system-linked" }, { "libcudart.so.12", "12.x" } };
        s.cudnnCandidates = { "cudnn", "libcudnn.so.8", "libcudnn.so.9" };
        s.ortCudaProviderLib = "libonnxruntime_providers_cuda.so";
        s.nvidiaSmiExecutable = "nvidia-smi";
#endif
        // macOS 及其它平台：detectable/installable 均为 false，所有候选保持为空
        return s;
    }();
    return spec;
}


struct GpuDetectionResult {
    bool hasNvidiaGpu = false;
    QString gpuName;
    bool hasCudaRuntime = false;
    QString cudaRuntimeVersion;
    bool hasCudnn = false;
    bool hasOrtCudaProvider = false;
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
