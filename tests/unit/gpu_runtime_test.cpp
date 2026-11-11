#include <gtest/gtest.h>

#include <QEventLoop>
#include <QTimer>

#include "sherpa/GpuRuntime.h"

namespace {

GpuDetectionResult readyDetection()
{
    GpuDetectionResult result;
    result.hasNvidiaGpu = true;
    result.gpuName = QStringLiteral("Fake GPU");
    result.hasCudaRuntime = true;
    result.hasOrtCudaProvider = true;
    result.isFullyReady = true;
    return result;
}

GpuDetectionResult gpuWithoutBackend()
{
    GpuDetectionResult result;
    result.hasNvidiaGpu = true;
    result.gpuName = QStringLiteral("Fake GPU");
    result.failReason = QStringLiteral("backend missing");
    return result;
}

bool waitForDetection(GpuRuntime& runtime, int timeoutMs = 5000)
{
    QEventLoop loop;
    bool finished = false;
    QObject::connect(&runtime, &GpuRuntime::detectionFinished, &loop, [&]() {
        finished = true;
        loop.quit();
    });
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    runtime.detectAsync();
    loop.exec();
    return finished;
}

} // namespace

TEST(GpuRuntime, PlatformWithoutDetectionCountsAsDone)
{
    GpuRuntime runtime;
    EXPECT_EQ(runtime.detectionDone(), !runtime.detectable());
    EXPECT_FALSE(runtime.backendReady());
}

TEST(GpuRuntime, ReadyBackendKeepsGpuRequest)
{
    GpuRuntime runtime;
    if (!runtime.detectable()) GTEST_SKIP() << "CUDA detection is not available on this platform";

    runtime.setDetector([] { return readyDetection(); });
    runtime.setRequestedGpu(true);
    ASSERT_TRUE(waitForDetection(runtime));

    EXPECT_TRUE(runtime.detectionDone());
    EXPECT_TRUE(runtime.backendReady());
    EXPECT_TRUE(runtime.requestedGpu());
}

TEST(GpuRuntime, MissingBackendLocksCpuMode)
{
    GpuRuntime runtime;
    if (!runtime.detectable()) GTEST_SKIP() << "CUDA detection is not available on this platform";

    runtime.setDetector([] { return gpuWithoutBackend(); });
    runtime.setRequestedGpu(true);
    ASSERT_TRUE(waitForDetection(runtime));

    EXPECT_TRUE(runtime.detectionDone());
    EXPECT_FALSE(runtime.backendReady());
    EXPECT_FALSE(runtime.requestedGpu());

    // 检测结论落定后，再点"切换 GPU"也不能越过未就绪状态
    runtime.setRequestedGpu(true);
    EXPECT_FALSE(runtime.requestedGpu());
}

TEST(GpuRuntime, MismatchOnlyWhenCudaWasRequested)
{
    GpuRuntime runtime;

    runtime.reportLoadResult({QStringLiteral("repo/gpu"), GpuProvider::Cpu, true, true});
    EXPECT_TRUE(runtime.lastLoadMismatch());
    EXPECT_EQ(runtime.actualProvider(), GpuProvider::Cpu);

    runtime.reportLoadResult({QStringLiteral("repo/gpu"), GpuProvider::Cuda, true, true});
    EXPECT_FALSE(runtime.lastLoadMismatch());
    EXPECT_EQ(runtime.actualProvider(), GpuProvider::Cuda);

    // int8 没有 GPU 变体：本来就不请求 CUDA，用 CPU 不算回退
    runtime.reportLoadResult({QStringLiteral("repo/cpu"), GpuProvider::Cpu, false, true});
    EXPECT_FALSE(runtime.lastLoadMismatch());
}

TEST(GpuRuntime, ModeChangeClearsLastProvider)
{
    GpuRuntime runtime;
    if (!runtime.detectable()) GTEST_SKIP() << "CUDA detection is not available on this platform";

    runtime.setRequestedGpu(true);
    runtime.reportLoadResult({QStringLiteral("repo/gpu"), GpuProvider::Cuda, true, true});
    ASSERT_EQ(runtime.actualProvider(), GpuProvider::Cuda);

    runtime.setRequestedGpu(false);
    EXPECT_EQ(runtime.actualProvider(), GpuProvider::Unknown);
    EXPECT_FALSE(runtime.lastLoadMismatch());
}
