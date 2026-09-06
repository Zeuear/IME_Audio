#include "CudaInstaller.h"
#include "CudaPlatformSpec.h"
#include <QLibrary>



CudaInstaller::CudaInstaller(QNetworkAccessManager* nam, SherpaInstaller* sherpaInstaller, QObject* parent) : QObject(parent)
{
    m_cudaDownloader = new DownloadManager(nam);
    m_taskManager = new TaskQueueManager(this);
    m_sherpaInstaller = sherpaInstaller;

    connect(m_cudaDownloader, &DownloadManager::groupFileProgress, this, &CudaInstaller::onGroupFileProgress);
    connect(m_cudaDownloader, &DownloadManager::groupFileFinished, this, &CudaInstaller::onGroupFileFinished);
    connect(m_cudaDownloader, &DownloadManager::groupFileError, this, &CudaInstaller::onGroupFileError);
    connect(m_cudaDownloader, &DownloadManager::groupFinished, this, &CudaInstaller::onGroupFinished);
}

CudaInstaller::~CudaInstaller() {}



void CudaInstaller::setEnvironment() {
    if (!cudaPlatformSpec().installable) {
        return;
    }
    QString cudnnBinDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/CUDNN/v9.x/bin";
    if (!QFile::exists(cudnnBinDir)) {
        return;
    }
    QString path = qEnvironmentVariable("PATH");
    QString nativeCudnnPath = QDir::toNativeSeparators(cudnnBinDir);

    if (!path.contains(nativeCudnnPath, Qt::CaseInsensitive)) {
        // 动态修改当前进程及其子进程的环境变量
        qputenv("PATH", (nativeCudnnPath + ";" + path).toLocal8Bit());
    }
}

GpuDetectionResult CudaInstaller::detectGpuEnvironment(bool requireCudnn)
{
    GpuDetectionResult result;
    const CudaPlatformSpec& spec = cudaPlatformSpec();
    if (!spec.detectable) {
        result.failReason = tr("CUDA is not available on this platform");
        return result;
    }

    // 第一层:硬件检测
    QString gpuName;
    if (!detectNvidiaGpuPresent(&gpuName)) {
        result.hasNvidiaGpu = false;
        result.failReason = tr("NVIDIA GPU or driver not detected");
        return result;
    }
    result.hasNvidiaGpu = true;
    result.gpuName = gpuName;

    // 第二层:CUDA Runtime 动态库检测
    bool cudartLoaded = false;
    for (const auto& cand : spec.cudartCandidates) {
        QLibrary lib(cand.libName);
        if (lib.load()) {
            if (lib.resolve("cudaGetDeviceCount") != nullptr) {
                result.hasCudaRuntime = true;
                result.cudaRuntimeVersion = cand.version;
                cudartLoaded = true;
                lib.unload();
                break;
            }
            lib.unload();
        }
    }

    if (!cudartLoaded) {
        result.hasCudaRuntime = false;
        result.failReason = QString("Detected NVIDIA GPU, but no available CUDA Runtime (cudart) found. Please ensure CUDA Toolkit is installed or download the corresponding backend.");
        return result;
    }

    // 第三层(按需):cuDNN 检测,如果 sherpa-onnx 的 GPU 推理依赖 cudnn
    if (requireCudnn) {
        bool cudnnLoaded = false;
        for (const auto& name : spec.cudnnCandidates) {
            QLibrary lib(name);
            if (lib.load()) {
                if (lib.resolve("cudnnGetVersion") != nullptr) {
                    cudnnLoaded = true;
                    lib.unload();
                    break;
                }
                lib.unload();
            }
        }
        result.hasCudnn = cudnnLoaded;
        if (!cudnnLoaded) {
            result.failReason = tr("Detected CUDA Runtime, but no available cuDNN found. GPU acceleration requires additional cuDNN installation.");
        }
    }

    // 第四层:ONNX Runtime CUDA Execution Provider 检测
    {
        QString dllPath = QApplication::applicationDirPath() + QDir::separator() + spec.ortCudaProviderLib;
        result.hasOrtCudaProvider = QFile::exists(dllPath);

        //QLibrary ortLib(ortCudaLib);
        //if (result.hasOrtCudaProvider) {
        //    ortLib.unload();
        //}
        //else {
        //    result.failReason = tr("Detected CUDA Runtime, but ONNX Runtime CUDA provider (onnxruntime_providers_cuda) not found. GPU acceleration requires the sherpa-onnx CUDA build.");
        //}
    }

    result.isFullyReady = result.hasNvidiaGpu && result.hasCudaRuntime
        && result.hasOrtCudaProvider
        && (!requireCudnn || result.hasCudnn);
    return result;
}

bool CudaInstaller::detectNvidiaGpuPresent(QString* gpuNameOut)
{
    const CudaPlatformSpec& spec = cudaPlatformSpec();
    if (!spec.detectable) {
        return false;
    }

    QProcess process;
    process.start(spec.nvidiaSmiExecutable, { "--query-gpu=name", "--format=csv,noheader" });

    if (!process.waitForStarted(1500)) {
        return false;
    }
    if (!process.waitForFinished(3000)) {
        process.kill();
        return false;
    }

    if (process.exitCode() != 0) {
        return false;
    }

    QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    if (output.isEmpty()) {
        return false;
    }
    QString firstLine = output.split('\n').first().trimmed();
    if (gpuNameOut) *gpuNameOut = firstLine;
    return true;
}

void CudaInstaller::startDownload(const GpuDetectionResult& result)
{
    const CudaPlatformSpec& spec = cudaPlatformSpec();
    if (!spec.installable) {
        emit installGroupFinished(GROUP_ID, false, tr("CUDA installation is not supported on this platform"));
        return;
    }

    m_detail = result;
    emit statusChanged(QString(tr("Preparation for CUDA 12.6 offline installation package...")));
    QString downloadDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (downloadDir.isEmpty()) {
        downloadDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }

    m_cudaInstallerPath = QDir(downloadDir).filePath(spec.cudaInstallerFileName);
    QUrl cudaUrl(spec.cudaInstallerUrl);

    m_cudnnZipPath = QDir(downloadDir).filePath(spec.cudnnArchiveFileName);
    QUrl cudnnUrl(spec.cudnnArchiveUrl);

    m_sherpaZipPath = QDir(downloadDir).filePath(spec.sherpaArchiveFileName);
    QUrl sherpaUrl(spec.sherpaArchiveUrl);

    int count = 0;
    if (!result.hasCudaRuntime) {
        m_cudaDownloader->addGroupTask(GROUP_ID, "CUDA 12.6", cudaUrl, m_cudaInstallerPath);
        count++;
    }

    if (!result.hasCudnn) {
        m_cudaDownloader->addGroupTask(GROUP_ID, "CUDNN 9.6", cudnnUrl, m_cudnnZipPath);
        count++;
    }

    if (!result.hasOrtCudaProvider) {
        m_cudaDownloader->addGroupTask(GROUP_ID, "Sherpa Runtime", sherpaUrl, m_sherpaZipPath);
        count++;
    }

    emit installGroupStarted(GROUP_ID, "GPU", count);
}

void CudaInstaller::cancelDownload()
{
    m_cudaDownloader->cancelAll();
}

void CudaInstaller::onGroupFileProgress(const QString& groupId, const QString&, const QString& filename,
    qint64 received, qint64 total, int overallPercent)
{
    emit installFileProgress(groupId, filename, received, total, overallPercent);
}

void CudaInstaller::onGroupFileFinished(const QString& groupId, const QString&, const QString& filename)
{
    emit installFileFinished(groupId, filename);
}

void CudaInstaller::onGroupFileError(const QString& groupId, const QString&, const QString& filename, const QString& error)
{
    emit installFileError(groupId, filename, error);
}

void CudaInstaller::onGroupFinished(const QString& groupId, bool success)
{
    if (!success) {
        emit installGroupFinished(groupId, false, tr("Download Error"));
        return;
    }
    else {
        emit installStarted();
        startInstall();
    }
}

void CudaInstaller::startInstall()
{
    emit statusChanged(tr("Starting installation sequence..."));
    connect(m_taskManager, &TaskQueueManager::allTasksFinished, this, [this](bool success, const QString& msg) {
        LOG_INFO("All tasks completed");
        LOG_DEBUG("Task All Complete");
        if (QFile::exists(m_cudaInstallerPath)) {
            QFile::remove(m_cudaInstallerPath);
        }
        if (QFile::exists(m_cudnnZipPath)) {
            QFile::remove(m_cudnnZipPath);
        }

        if (success) {
            finishInstallation(true, tr("Full CUDA + cuDNN installation completed."));
        }
        else {
            finishInstallation(false, tr("Installation sequence failed: ") + msg);
        }
        });

    if (QFile::exists(m_cudaInstallerPath) && !m_detail.hasCudaRuntime) {
        ElevatedProcessTask* cudaTask = new ElevatedProcessTask(
            m_cudaInstallerPath, cudaPlatformSpec().cudaInstallerArgs, m_taskManager);
        connect(cudaTask, &ElevatedProcessTask::installProgress, this, [this](const QString& msg) { LOG_INFO(msg); });

        if (!m_detail.hasCudaRuntime) {
            m_taskManager->addTask(cudaTask);
        }
    }

    if (QFile::exists(m_cudnnZipPath) && !m_detail.hasCudnn) {
        QString cudnnInstallDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/CUDNN/v9.x";
        QDir().mkpath(cudnnInstallDir);
        ExtractTask* cudnnTask = new ExtractTask(m_cudnnZipPath, cudnnInstallDir);
        connect(cudnnTask, &ExtractTask::extractStarted, this, [this](int total) { emit extractStarted(GROUP_ID, total); });
        connect(cudnnTask, &ExtractTask::extractProgress, this, [this](int cur, int tot) { emit extractProgress(GROUP_ID, cur, tot); });
        connect(cudnnTask, &ExtractTask::extractFinished, this, [this](bool ok) { emit extractFinished(GROUP_ID, ok); });
        if (!m_detail.hasCudnn) {
            m_taskManager->addTask(cudnnTask);
        }
    }

    if (QFile::exists(m_sherpaZipPath)) {

        ExtractOptions opts;
        opts.destinationDir = QApplication::applicationDirPath();
        opts.filter = [](const QFileInfo& fi) {
            return fi.fileName().startsWith("onnx", Qt::CaseInsensitive);
            };
        ExtractExTask* sherpaTask = new ExtractExTask(m_sherpaZipPath, opts);
        connect(sherpaTask, &ExtractExTask::extractStarted, this, [this](int total) { emit extractStarted(GROUP_ID, total); });
        connect(sherpaTask, &ExtractExTask::extractProgress, this, [this](int cur, int tot) { emit extractProgress(GROUP_ID, cur, tot); });
        connect(sherpaTask, &ExtractExTask::extractFinished, this, [this](bool ok) { emit extractFinished(GROUP_ID, ok); });
        m_taskManager->addTask(sherpaTask);
    }
}

void CudaInstaller::finishInstallation(bool success, const QString& message)
{
    emit installFinished(success, success ? QString() : message);
}


