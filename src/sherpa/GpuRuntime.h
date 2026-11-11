#pragma once
#include <QFutureWatcher>
#include <QMetaType>
#include <QObject>
#include <QString>

#include <functional>

#include "sherpa/GpuDetection.h"
#include "sherpa/GpuVariant.h"

class CudaInstaller;

// @brief 管理 GPU 检测、安装及实际运行状态。
class GpuRuntime : public QObject {
    Q_OBJECT
public:
    using Detector = std::function<GpuDetectionResult()>;

    explicit GpuRuntime(QObject* parent = nullptr);
    ~GpuRuntime() override;

    void setDetector(Detector detector);
    void setInstaller(CudaInstaller* installer);

    void detectAsync();
    void redetect();
    void startInstall();
    void cancelInstall();
    void setEnvironment();

    bool detectable() const;
    bool detecting() const;
    bool detectionDone() const;
    bool backendReady() const;
    const GpuDetectionResult& detection() const;
    bool installing() const;

    bool requestedGpu() const;
    void setRequestedGpu(bool on);

    GpuProvider actualProvider() const;
    QString actualRepoId() const;
    bool lastLoadMismatch() const;

    void reportLoadResult(const GpuLoadResult& result);

signals:
    void stateChanged();
    void detectionFinished();
    void installGroupStarted(const QString& groupId, const QString& displayName, int totalFiles);
    void installGroupFinished(const QString& groupId, bool success, const QString& message);
    void installFileProgress(const QString& groupId, const QString& filename,
                             qint64 received, qint64 total, int overallPercent);
    void installFileFinished(const QString& groupId, const QString& filename);
    void installFileError(const QString& groupId, const QString& filename, const QString& error);
    void extractStarted(const QString& groupId, int totalLines);
    void extractProgress(const QString& groupId, int current, int total);
    void extractFinished(const QString& groupId, bool success);
    void statusChanged(const QString& status);
    void progressUpdated(int percent);
    void installStarted();
    void installFinished(bool success, const QString& message);

private:
    void finishDetection(const GpuDetectionResult& result);

    Detector m_detector;
    QFutureWatcher<GpuDetectionResult>* m_watcher = nullptr;
    CudaInstaller* m_installer = nullptr;

    GpuDetectionResult m_detection;
    bool m_detecting = false;
    bool m_detectionDone = false;
    bool m_installing = false;
    bool m_requestedGpu = false;

    GpuProvider m_actualProvider = GpuProvider::Unknown;
    QString m_actualRepoId;
    bool m_lastLoadMismatch = false;
};

Q_DECLARE_METATYPE(GpuLoadResult)
