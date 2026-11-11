#pragma once
#include <functional>

// 平台隐私授权（TCC）的查询与申请。
class PlatformPermissions {
public:
    enum class Status {
        NotRequired,    // 本平台无此授权概念，直接可用
        Granted,
        Denied,         // 用户已拒绝或被策略限制：系统不会再弹框，只能引导去系统设置
        NotDetermined,  // 尚未询问过：可以弹一次系统授权框
    };

    // ---- 麦克风（录音链路） ----
    static Status microphoneStatus();

    // 异步申请
    static void requestMicrophoneAccess(std::function<void(bool granted)> callback);

    // 打开系统设置中的麦克风隐私面板
    static void openMicrophoneSettings();

    // ---- 辅助功能（合成键盘事件 / 文本注入） ----
    static Status accessibilityStatus();

    // prompt=true 时，未授权会弹出系统的「打开系统设置」引导框
    static bool requestAccessibility(bool prompt);

    static void openAccessibilitySettings();

    // 授权要重启进程才生效（macOS）
    static void relaunchApp();
};
