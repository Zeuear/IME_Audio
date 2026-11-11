#pragma once
#include <QFutureWatcher>
#include <QObject>
#include <QString>

#include <functional>

#include "sherpa/GpuDetection.h"
#include "sherpa/GpuVariant.h"

// @brief 管理 GPU 检测结果与运行状态
class GpuRuntime : public QObject {
    Q_OBJECT
public:
    using Detector = std::function<GpuDetectionResult()>;

    explicit GpuRuntime(QObject* parent = nullptr);
    ~GpuRuntime() override;

    void setDetector(Detector detector);

    // 正在检测时重复调用会被忽略。
    void detectAsync();
    void redetect();

    bool detectable() const;
    bool detecting() const;
    // 不支持检测的平台也视为检测完成。
    bool detectionDone() const;
    bool backendReady() const;
    const GpuDetectionResult& detection() const;

    bool requestedGpu() const;
    // 不支持检测或后端未就绪时会回退到 CPU。
    void setRequestedGpu(bool on);

    GpuProvider actualProvider() const;
    QString actualRepoId() const;
    // 最近一次请求 CUDA 的加载未使用 CUDA，或加载失败。
    bool lastLoadMismatch() const;

    // 加载失败时 actual 应为 Unknown；requestedCuda 表示本次是否请求 CUDA。
    void reportLoadResult(const QString& repoId, GpuProvider actual, bool requestedCuda);

signals:
    void stateChanged();
    void detectionFinished();

private:
    void finishDetection(const GpuDetectionResult& result);

    Detector m_detector;
    QFutureWatcher<GpuDetectionResult>* m_watcher = nullptr;

    GpuDetectionResult m_detection;
    bool m_detecting = false;
    bool m_detectionDone = false;
    bool m_requestedGpu = false;

    GpuProvider m_actualProvider = GpuProvider::Unknown;
    QString m_actualRepoId;
    bool m_lastLoadMismatch = false;
};
