#pragma once
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <functional>

// @brief 模型 CPU 版与 GPU 变体之间的配对规则，纯逻辑、不依赖识别库，便于单元测试。
namespace GpuVariant {

struct Link {
    QString repoId;
    QString gpuRepoId;        // 非空 = 该 CPU 模型有 GPU 变体
    bool gpuVariant = false;  // 该行本身是 GPU 变体
};

// int8 模型在 ORT 的 CUDA 后端没有算子内核，只有 GPU 变体才值得用 CUDA
inline bool shouldUseCuda(bool gpuVariant, bool globalUseGpu)
{
    return globalUseGpu && gpuVariant;
}

inline bool isGpuVariant(const QList<Link>& links, const QString& repoId)
{
    for (const Link& l : links)
        if (l.repoId == repoId) return l.gpuVariant;
    return false;
}

inline QString gpuVariantOf(const QList<Link>& links, const QString& cpuRepoId)
{
    for (const Link& l : links)
        if (l.repoId == cpuRepoId) return l.gpuRepoId;
    return {};
}

inline QString cpuVariantOf(const QList<Link>& links, const QString& gpuRepoId)
{
    if (gpuRepoId.isEmpty()) return {};
    for (const Link& l : links)
        if (l.gpuRepoId == gpuRepoId) return l.repoId;
    return {};
}

inline QList<QPair<QString, QString>> onnxFileList(const QString& convFile, const QString& encoderFile,
                                                   const QString& decoderFile, const QString& decoderDataFile,
                                                   const QString& tokenizerDir, const QStringList& tokenizerFiles)
{
    QList<QPair<QString, QString>> out{ {{}, convFile}, {{}, encoderFile}, {{}, decoderFile} };
    if (!decoderDataFile.isEmpty()) out.append({{}, decoderDataFile});
    for (const QString& t : tokenizerFiles) out.append({tokenizerDir, t});
    return out;
}


// 启动时首次加载模型要不要等 GPU 检测：只有 useGpu 为真且检测还没结束才等，useGpu 为假时立即加载
inline bool shouldWaitForGpuDetection(bool useGpu, bool detectionDone)
{
    return useGpu && !detectionDone;
}

// 该 CPU 模型的 GPU 加速组件状态：没有 GPU 版 / 有但没下载 / 已下载
enum class GpuState { Unsupported, NotInstalled, Ready };

inline GpuState gpuStateOf(const QList<Link>& links, const QString& cpuRepoId,
                           const std::function<bool(const QString&)>& isInstalled)
{
    const QString gpu = gpuVariantOf(links, cpuRepoId);
    if (gpu.isEmpty()) return GpuState::Unsupported;
    return isInstalled(gpu) ? GpuState::Ready : GpuState::NotInstalled;
}

// 实际加载哪个仓库
inline QString resolveForMode(const QList<Link>& links, const QString& repoId, bool useGpu,
                              const std::function<bool(const QString&)>& isInstalled)
{
    if (useGpu && gpuStateOf(links, repoId, isInstalled) == GpuState::Ready)
        return gpuVariantOf(links, repoId);
    return repoId;
}

}
