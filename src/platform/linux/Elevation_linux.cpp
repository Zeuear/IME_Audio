#include "../../utils/ProcessManager.h"

// Linux 不提供提权安装：CUDA 由发行版包管理器安装，见 CudaPlatformSpec 的 installable=false。
void ElevatedProcessTask::runElevatedBlocking() {
    emit taskFinished(TaskResult::Failed, "Elevated install is not supported on Linux");
    emit elevatedFinished();
}
