#pragma once
#include "services/WorkflowManager.h"

// @brief 横条按钮的可用性，只由工作流状态决定，不含任何界面代码，便于单元测试。
namespace ControlBarState {

struct Buttons {
    bool startEnabled = false;
    bool stopEnabled = false;
};

// 开始键只在空闲时可点；停止键在加载、录音、转录、后处理时可点；
// 正在停止或出错时两个都不可点，避免重复触发
Buttons forWorkflow(WorkflowState state);

} // namespace ControlBarState
