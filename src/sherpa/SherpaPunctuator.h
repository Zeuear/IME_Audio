#pragma once

#include <QObject>
#include <QString>
#include <memory>

#include "cxx-api.h"

namespace sherpa_onnx::cxx {
class OfflinePunctuation;
}

/**
 * @brief 神经标点恢复器（sherpa-onnx OfflinePunctuation 封装）。
 *
 * 与 ASR 识别器（OfflineRecognizer/OnlineRecognizer）完全独立：它只做
 * 文本 -> 文本 的标点恢复，是纯 CPU 的轻量推理。由 SherpaManager 在加载
 * 配对的 ASR 模型时一并加载，卸载时一并释放。
 *
 * 线程安全：load/unload/punctuate 均应在同一转录工作线程内调用
 *（与 SherpaManager 的 workerLoop 同线程），不做内部加锁。
 */
class SherpaPunctuator : public QObject {
    Q_OBJECT
public:
    explicit SherpaPunctuator(QObject* parent = nullptr);
    ~SherpaPunctuator() override;

    SherpaPunctuator(const SherpaPunctuator&) = delete;
    SherpaPunctuator& operator=(const SherpaPunctuator&) = delete;

    /**
     * @brief 加载标点模型（ct_transformer onnx 目录）。
     * @param modelDir 含 model.onnx 的目录（csukuangfj/...zh-en... 解压目录）
     * @return 是否成功加载
     */
    bool load(const QString& modelDir);

    /** @brief 是否已加载可用模型 */
    bool isLoaded() const;

    /**
     * @brief 为一段文本恢复标点（同步）。
     * @param text 源语言文本（通常应为 zh/en）
     * @return 已加标点的文本；未加载时原样返回（调用方兜底启发式）
     */
    QString punctuate(const QString& text);

    /** @brief 释放模型，回到未加载状态 */
    void unload();

private:
    std::unique_ptr<sherpa_onnx::cxx::OfflinePunctuation> m_punct;
};
