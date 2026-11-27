#pragma once
#include <functional>

// 平台隐私授权（TCC）的查询与申请。
class PlatformPermissions {
public:
    enum class Status {
        NotRequired,
        Granted,
        Denied,         // 用户已拒绝或被策略限制：系统不会再弹框，只能引导去系统设置
        NotDetermined,  // 尚未询问过：可以弹一次系统授权框
    };

    static Status microphoneStatus();

    static void requestMicrophoneAccess(std::function<void(bool granted)> callback);

    static void openMicrophoneSettings();

    static Status accessibilityStatus();

    static bool requestAccessibility(bool prompt);

    static void openAccessibilitySettings();

    // 授权要重启进程才生效（macOS）
    static void relaunchApp();
};
