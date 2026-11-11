#pragma once
#include <QString>

struct GpuDetectionResult {
    bool hasNvidiaGpu = false;
    QString gpuName;
    bool hasCudaRuntime = false;
    QString cudaRuntimeVersion;
    bool hasCudnn = false;
    bool hasOrtCudaProvider = false;
    bool isFullyReady = false;
    QString failReason;
};
