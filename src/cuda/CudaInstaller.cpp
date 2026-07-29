#include "CudaInstaller.h"
#include <QLibrary>
CudaInstaller::CudaInstaller(QNetworkAccessManager* nam, QObject* parent) : QObject(parent)
{
    m_cudaDownloader = new Downloader(nam, this);
    m_cudnnDownloader = new Downloader(nam, this);
    m_installProcess = new QProcess(this);

    // CUDA 下载信号
    connect(m_cudaDownloader, &Downloader::progress, this, &CudaInstaller::onCudaDownloadProgress);
    connect(m_cudaDownloader, &Downloader::finished, this, &CudaInstaller::onCudaDownloadFinished);
    connect(m_cudaDownloader, &Downloader::error, this, &CudaInstaller::onCudaDownloadError);

    // cuDNN 下载信号
    connect(m_cudnnDownloader, &Downloader::progress, this, &CudaInstaller::onCudnnDownloadProgress);
    connect(m_cudnnDownloader, &Downloader::finished, this, &CudaInstaller::onCudnnDownloadFinished);
    connect(m_cudnnDownloader, &Downloader::error, this, &CudaInstaller::onCudnnDownloadError);

    connect(m_installProcess, &QProcess::finished, this, &CudaInstaller::onProcessFinished);
}

CudaInstaller::~CudaInstaller()
{
}

GpuDetectionResult CudaInstaller::detectGpuEnvironment(bool requireCudnn)
{
    GpuDetectionResult result;
    // ---- 第一层:硬件检测 ----
    QString gpuName;
    if (!detectNvidiaGpuPresent(&gpuName)) {
        result.hasNvidiaGpu = false;
        result.failReason = tr("NVIDIA GPU or driver not detected");
        return result;
    }
    result.hasNvidiaGpu = true;
    result.gpuName = gpuName;

    // ---- 第二层:CUDA Runtime 动态库检测 ----
    // 不同平台/版本的 cudart 命名不同,按常见版本号依次尝试
    // Windows: cudart64_110.dll, cudart64_118.dll, cudart64_12.dll ...
    // Linux:   libcudart.so.11.0, libcudart.so.12, libcudart.so (符号链接)
    struct CudaLibCandidate { QString libName; QString version; };

    QVector<CudaLibCandidate> candidates;
#ifdef Q_OS_WIN
    candidates = {
        {"cudart64_12", "12.x"},
        {"cudart64_13", "13.x"},
    };
#elif defined(Q_OS_LINUX)
    candidates = {
        {"cudart", "system-linked"},
        {"libcudart.so.12", "12.x"},
    };
#endif

    bool cudartLoaded = false;
    for (const auto& cand : candidates) {
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

    // ---- 第三层(按需):cuDNN 检测,如果 sherpa-onnx 的 GPU 推理依赖 cudnn ----
    if (requireCudnn) {
        QVector<QString> cudnnCandidates;
#ifdef Q_OS_WIN
        cudnnCandidates = { "cudnn64_9" };
#elif defined(Q_OS_LINUX)
        cudnnCandidates = { "cudnn", "libcudnn.so.8", "libcudnn.so.9" };
#endif
        bool cudnnLoaded = false;
        for (const auto& name : cudnnCandidates) {
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

    result.isFullyReady = result.hasNvidiaGpu && result.hasCudaRuntime
        && (!requireCudnn || result.hasCudnn);
    return result;
}

bool CudaInstaller::detectNvidiaGpuPresent(QString* gpuNameOut)
{
    QProcess process;
#ifdef Q_OS_WIN
    process.start("nvidia-smi.exe", { "--query-gpu=name", "--format=csv,noheader" });
#else
    process.start("nvidia-smi", { "--query-gpu=name", "--format=csv,noheader" });
#endif

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

void CudaInstaller::startInstallCuda()
{
    emit statusChanged(QString(tr("Preparation for CUDA 12.6 offline installation package...")));
    QString downloadDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (downloadDir.isEmpty()) {
        downloadDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    
    m_cudaInstallerPath = QDir(downloadDir).filePath("cuda_12.6.0_560.76_windows.exe");
    QUrl cudaUrl("https://developer.download.nvidia.com/compute/cuda/12.6.0/local_installers/cuda_12.6.0_560.76_windows.exe");
  
    m_cudnnZipPath = QDir(downloadDir).filePath("cudnn-windows-x86_64-9.6.0.29_cuda12-archive.zip");
    QUrl cudnnUrl("https://developer.download.nvidia.com/compute/cudnn/redist/cudnn/windows-x86_64/cudnn-windows-x86_64-9.6.0.29_cuda12-archive.zip");

    m_cudaDownloader->start(cudaUrl, m_cudaInstallerPath);
    m_cudnnDownloader->start(cudnnUrl, m_cudnnZipPath);
}


void CudaInstaller::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    bool cudaSuccess = (exitStatus == QProcess::NormalExit && exitCode == 0);

    QFile::remove(m_cudaInstallerPath);   // 清理安装包

    if (!cudaSuccess) {
        emit installFileError(GROUP_ID, "CUDA 12.6 Installer.exe", tr("Installation failed with code %1").arg(exitCode));
        finishInstallation(false, tr("CUDA installation failed"));
        return;
    }

    emit installFileFinished(GROUP_ID, "CUDA 12.6 Installer.exe");  // 标记 CUDA 安装完成
    emit statusChanged(tr("CUDA installed successfully. Installing cuDNN..."));

    startCudnnInstallation();
}

void CudaInstaller::onCudaDownloadProgress(const QString& fileName, qint64 received, qint64 total, double speed)
{
    int percent = total > 0 ? static_cast<int>(received * 100 / total) : 0;
    int overall = (percent * 65) / 100;   // CUDA 占整体进度的 65%
    emit installFileProgress(GROUP_ID, "CUDA 12.6 Installer.exe", received, total, overall);
}

void CudaInstaller::onCudnnDownloadProgress(const QString& /*fileName*/, qint64 received, qint64 total, double /*speed*/)
{
    int filePercent = total > 0 ? static_cast<int>(received * 100 / total) : 0;
    int overall = 70 + (filePercent * 30) / 100;   // cuDNN 占剩下30%
    emit installFileProgress(GROUP_ID, "cuDNN 9.x", received, total, overall);
}

void CudaInstaller::onCudaDownloadFinished(const QString&)
{
    m_cudaDownloaded = true;
    emit installFileFinished(GROUP_ID, "CUDA 12.6 Installer.exe");
    checkDownloadComplete();
}

void CudaInstaller::onCudnnDownloadFinished(const QString& savePath)
{
    m_cudnnDownloaded = true;
    emit installFileFinished(GROUP_ID, "cuDNN 9.x Package");
    checkDownloadComplete();
}

void CudaInstaller::onCudaDownloadError(const QString& err)
{
    emit installFileError(GROUP_ID, "CUDA 12.6 Installer.exe", err);
    finishInstallation(false, tr("CUDA download failed: ") + err);
}

void CudaInstaller::onCudnnDownloadError(const QString& errorString)
{
    emit installFinished(false, tr("cuDNN Download Error: %1").arg(errorString));
}


void CudaInstaller::checkDownloadComplete()
{
    if (m_cudaDownloaded && m_cudnnDownloaded && !m_isInstalling) {
        m_isInstalling = true;
        emit statusChanged(tr("Download completed. Starting installation..."));
        startCudaInstallation();
    }
}

void CudaInstaller::startCudaInstallation()
{
    emit statusChanged(tr("Installing CUDA 12.6 (this may take several minutes)..."));

    QStringList args = { "/s", "-n", "nvcc_12.6", "cusparse_12.6", "cublas_12.6", "cudart_12.6" };

    m_installProcess->setProgram(m_cudaInstallerPath);
    m_installProcess->setArguments(args);
    m_installProcess->start();
}

void CudaInstaller::startCudnnInstallation()
{
    emit statusChanged(tr("Extracting and installing cuDNN 9.x..."));

    // 解压并复制到标准目录
    QString tempExtractPath = QDir::tempPath() + "/cudnn_extract";
    QDir().mkpath(tempExtractPath);

    // 使用 PowerShell 解压
    QProcess unzip;
    unzip.setProgram("powershell");
    unzip.setArguments({
        "-NoProfile", "-Command",
        QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force")
            .arg(QDir::toNativeSeparators(m_cudnnZipPath))
            .arg(QDir::toNativeSeparators(tempExtractPath))
        });

    unzip.start();
    unzip.waitForFinished(120000); // 最长等待 2 分钟

    QString targetBase = "C:/Program Files/NVIDIA/CUDNN/v9.x";
    QDir(targetBase).mkpath(".");

    // 复制 bin、include、lib（实际文件夹名可能为 cudnn-*-archive）
    QDir extractDir(tempExtractPath);
    QStringList subDirs = extractDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (!subDirs.isEmpty()) {
        QString sourceRoot = extractDir.absoluteFilePath(subDirs.first());
        // 复制 bin、include、lib
        QProcess::execute("xcopy", { sourceRoot + "\\bin", targetBase + "\\bin", "/E", "/I", "/Y" });
        QProcess::execute("xcopy", { sourceRoot + "\\include", targetBase + "\\include", "/E", "/I", "/Y" });
        QProcess::execute("xcopy", { sourceRoot + "\\lib", targetBase + "\\lib", "/E", "/I", "/Y" });
    }

    QFile::remove(m_cudnnZipPath);
    QDir(tempExtractPath).removeRecursively();

    finishInstallation(true, tr("CUDA + cuDNN installed successfully"));
}

void CudaInstaller::finishInstallation(bool success, const QString& message)
{
    emit installGroupFinished(GROUP_ID, success, success ? QString() : message);
    emit installFinished(success, message);
}


void CudaInstaller::cancelInstallCuda()
{
    m_cudaDownloader->cancel();
    m_cudnnDownloader->cancel();
    if (m_installProcess->state() != QProcess::NotRunning)
        m_installProcess->kill();
}
