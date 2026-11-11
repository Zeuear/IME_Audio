#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

struct CudaLibCandidate {
    QString libName;
    QString version;
};

struct CudaPlatformSpec {
    bool detectable = false;
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

inline const CudaPlatformSpec& cudaPlatformSpec()
{
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
        // Linux 使用发行版包管理器安装 CUDA，本程序只负责检测。
        s.detectable = true;
        s.installable = false;
        s.cudartCandidates = { { "cudart", "system-linked" }, { "libcudart.so.12", "12.x" } };
        s.cudnnCandidates = { "cudnn", "libcudnn.so.8", "libcudnn.so.9" };
        s.ortCudaProviderLib = "libonnxruntime_providers_cuda.so";
        s.nvidiaSmiExecutable = "nvidia-smi";
#endif
        return s;
    }();
    return spec;
}
