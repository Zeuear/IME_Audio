#include "../../utils/ProcessManager.h"

// Linux 不提供提权安装
void ElevatedProcessTask::runElevatedBlocking() {
    emit taskFinished(TaskResult::Failed, "Elevated install is not supported on Linux");
    emit elevatedFinished();
}
