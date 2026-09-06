#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// CUDA 相关的平台差异集中在这里。无 CUDA 的平台用显式的 detectable=false 表达，
// 而不是留下空候选列表让检测逻辑静默地什么都找不到。
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
        // Linux 能探测系统上已有的 CUDA，但本程序不提供 Linux 安装包，
        // 交由发行版包管理器安装（此前会误下载 Windows 的 .exe）。
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
