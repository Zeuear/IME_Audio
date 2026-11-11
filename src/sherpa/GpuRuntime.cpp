#include "GpuRuntime.h"

#include <QtConcurrent/QtConcurrent>

#include "installers/CudaPlatform.h"
#include "utils/Logger.h"

namespace {

GpuDetectionResult defaultDetection()
{
    GpuDetectionResult result;
    result.failReason = QStringLiteral("GPU detector is not wired up");
    return result;
}

} // namespace

GpuRuntime::GpuRuntime(QObject* parent)
    : QObject(parent)
    , m_detector(&defaultDetection)
    , m_detectionDone(!cudaPlatformSpec().detectable)
{
}

GpuRuntime::~GpuRuntime() = default;

void GpuRuntime::setDetector(Detector detector)
{
    m_detector = detector ? std::move(detector) : Detector(&defaultDetection);
}

bool GpuRuntime::detectable() const
{
    return cudaPlatformSpec().detectable;
}

bool GpuRuntime::detecting() const
{
    return m_detecting;
}

bool GpuRuntime::detectionDone() const
{
    return m_detectionDone || !detectable();
}

bool GpuRuntime::backendReady() const
{
    return detectionDone() && m_detection.isFullyReady;
}

bool GpuRuntime::installing() const
{
    return m_installing;
}

const GpuDetectionResult& GpuRuntime::detection() const
{
    return m_detection;
}

bool GpuRuntime::requestedGpu() const
{
    return m_requestedGpu;
}

void GpuRuntime::setRequestedGpu(bool on)
{
    if (!detectable()) on = false;
    // 检测已经结束且后端没装好：不允许停在 CUDA 模式
    if (on && detectionDone() && !backendReady()) on = false;
    if (m_requestedGpu == on) return;

    m_requestedGpu = on;
    m_actualProvider = GpuProvider::Unknown;
    m_actualRepoId.clear();
    m_lastLoadMismatch = false;
    emit stateChanged();
}

GpuProvider GpuRuntime::actualProvider() const
{
    return m_actualProvider;
}

QString GpuRuntime::actualRepoId() const
{
    return m_actualRepoId;
}

bool GpuRuntime::lastLoadMismatch() const
{
    return m_lastLoadMismatch;
}

void GpuRuntime::reportLoadResult(const GpuLoadResult& result)
{
    m_actualRepoId = result.repoId;
    m_actualProvider = result.success ? result.provider : GpuProvider::Unknown;
    m_lastLoadMismatch = result.requestedCuda && m_actualProvider != GpuProvider::Cuda;
    if (m_lastLoadMismatch) {
        LOG_WARN(QString("GPU load mismatch: requested CUDA but actual provider for %1 is %2")
                     .arg(result.repoId, m_actualProvider == GpuProvider::Cpu ? "cpu" : "unknown"));
    }
    emit stateChanged();
}

void GpuRuntime::detectAsync()
{
    if (!detectable()) {
        m_detectionDone = true;
        emit detectionFinished();
        emit stateChanged();
        return;
    }
    if (m_detecting) return;

    m_detecting = true;
    emit stateChanged();

    if (m_watcher) {
        m_watcher->cancel();
        m_watcher->deleteLater();
    }

    m_watcher = new QFutureWatcher<GpuDetectionResult>(this);
    connect(m_watcher, &QFutureWatcher<GpuDetectionResult>::finished, this, [this]() {
        finishDetection(m_watcher->result());
    });

    const Detector detector = m_detector;
    m_watcher->setFuture(QtConcurrent::run([detector]() { return detector(); }));
}

void GpuRuntime::finishDetection(const GpuDetectionResult& result)
{
    m_detection = result;
    m_detecting = false;
    m_detectionDone = true;

    // 后端没装好时把模式锁回 CPU：旧配置里的 useGpu=true 不能一直悬着
    if (m_requestedGpu && !m_detection.isFullyReady) {
        m_requestedGpu = false;
        m_actualProvider = GpuProvider::Unknown;
        m_actualRepoId.clear();
        m_lastLoadMismatch = false;
    }

    emit detectionFinished();
    emit stateChanged();
}
