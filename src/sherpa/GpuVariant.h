#pragma once
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <functional>

enum class GpuProvider { Unknown, Cpu, Cuda };

namespace GpuVariant {

struct Link {
    QString repoId;
    QString gpuRepoId;
    bool gpuVariant = false;
};

// int8 模型没有 CUDA 算子内核，因此只有 GPU 变体才使用 CUDA。
inline bool shouldUseCuda(bool gpuVariant, bool globalUseGpu)
{
    return globalUseGpu && gpuVariant;
}

inline bool isGpuVariant(const QList<Link>& links, const QString& repoId)
{
    for (const Link& link : links) {
        if (link.repoId == repoId) return link.gpuVariant;
    }
    return false;
}

inline QString gpuVariantOf(const QList<Link>& links, const QString& cpuRepoId)
{
    for (const Link& link : links) {
        if (link.repoId == cpuRepoId) return link.gpuRepoId;
    }
    return {};
}

inline QString cpuVariantOf(const QList<Link>& links, const QString& gpuRepoId)
{
    if (gpuRepoId.isEmpty()) return {};
    for (const Link& link : links) {
        if (link.gpuRepoId == gpuRepoId) return link.repoId;
    }
    return {};
}

inline QList<QPair<QString, QString>> onnxFileList(const QString& convFile, const QString& encoderFile,
                                                   const QString& decoderFile, const QString& decoderDataFile,
                                                   const QString& tokenizerDir, const QStringList& tokenizerFiles)
{
    QList<QPair<QString, QString>> files{ {{}, convFile}, {{}, encoderFile}, {{}, decoderFile} };
    if (!decoderDataFile.isEmpty()) files.append({{}, decoderDataFile});
    for (const QString& file : tokenizerFiles) files.append({tokenizerDir, file});
    return files;
}

inline bool shouldWaitForGpuDetection(bool useGpu, bool detectionDone)
{
    return useGpu && !detectionDone;
}

enum class GpuState { Unsupported, NotInstalled, Ready };

inline GpuState gpuStateOf(const QList<Link>& links, const QString& cpuRepoId,
                           const std::function<bool(const QString&)>& isInstalled)
{
    const QString gpu = gpuVariantOf(links, cpuRepoId);
    if (gpu.isEmpty()) return GpuState::Unsupported;
    return isInstalled(gpu) ? GpuState::Ready : GpuState::NotInstalled;
}

inline QString resolveForMode(const QList<Link>& links, const QString& repoId, bool useGpu,
                              const std::function<bool(const QString&)>& isInstalled)
{
    if (!useGpu) return repoId;

    const QString gpuRepoId = gpuVariantOf(links, repoId);
    if (!gpuRepoId.isEmpty() && isInstalled(gpuRepoId)) return gpuRepoId;

    return repoId;
}

}
