#include "../../utils/ProcessManager.h"

// macOS 不提供提权安装：CUDA 在本平台不存在，唯一的提权调用方（CudaInstaller）
// 已由 CudaPlatformSpec 在更上层拦截。
void ElevatedProcessTask::runElevatedBlocking() {
    emit taskFinished(TaskResult::Failed, "Elevated install is not supported on macOS");
    emit elevatedFinished();
}
