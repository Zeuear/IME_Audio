#pragma once
#include <QString>
#include "services/WorkflowManager.h"

// @brief 横条的纯逻辑：按钮可用性和录音计时文字，不含界面代码。
namespace ControlBarState {

struct Buttons {
    bool startEnabled = false;
    bool stopEnabled = false;
};

// 开始键只在空闲时可点；停止键在加载、录音、转录、后处理时可点；停止中或出错时都不可点，避免重复触发
Buttons forWorkflow(WorkflowState state);

// 一次听写会话是否在进行：计时从加载开始，到回到空闲或出错结束
bool isSessionActive(WorkflowState state);

// "mm:ss"，满一小时为 "h:mm:ss"，负数按 0
QString formatElapsed(qint64 seconds);

} // namespace ControlBarState
