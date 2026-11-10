#pragma once
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

// @brief 模型 CPU 版与 GPU 变体之间的配对规则，纯逻辑、不依赖识别库，便于单元测试。
namespace GpuVariant {

struct Link {
    QString repoId;
    QString gpuRepoId;        // 非空 = 该 CPU 模型有 GPU 变体
    bool gpuVariant = false;  // 该行本身是 GPU 变体
};

// 列表可见性：CPU 模式只列普通模型，GPU 模式只列 GPU 变体
inline bool visibleInMode(bool gpuVariant, bool gpuMode)
{
    return gpuVariant == gpuMode;
}

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

}
