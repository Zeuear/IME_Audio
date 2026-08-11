#ifndef UPDATE_POLICY_H
#define UPDATE_POLICY_H

// 更新提醒策略：是否应弹出"有更新可用"的提示。
// 用户勾选"不再提醒"后返回 false，避免反复打扰。
// 接受 bool 而非 AppConfig，避免测试 TU 拖入 QApplication 耦合。
inline bool shouldPromptUpdate(bool skipUpdateReminder)
{
    return !skipUpdateReminder;
}

#endif // UPDATE_POLICY_H
