#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

#if defined(Q_OS_WIN) && !defined(IMEAUDIO_SHERPA_VERSION)
#error "IMEAUDIO_SHERPA_VERSION 由 cmake/SherpaOnnx.cmake 经 sherpa_onnx 目标提供，请链接 sherpa_onnx"
#endif

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

        // 版本与包名由 cmake/SherpaOnnx.cmake 统一提供，必须和链接的 onnxruntime.dll 同一版，
        // 否则安装器的 provider 版本校验会失败
        s.sherpaArchiveFileName = "sherpa-onnx-v" IMEAUDIO_SHERPA_VERSION ".tar.bz2";
        s.sherpaArchiveUrl = "https://github.com/k2-fsa/sherpa-onnx/releases/download/v" IMEAUDIO_SHERPA_VERSION "/"
                             IMEAUDIO_SHERPA_WIN_CUDA_ARCHIVE;
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
