#include "../../utils/PlatformPermissions.h"

// Windows 上麦克风与模拟输入都不需要运行时授权：
// 麦克风的「应用权限」开关由系统在录音失败时自行提示，SendInput 也无需任何授权
// （UIPI 只影响向更高完整性级别的进程注入，那是提权问题，不是隐私授权）。
// 因此这里全部报告 NotRequired，让上层直接走原路径。
PlatformPermissions::Status PlatformPermissions::microphoneStatus() {
    return Status::NotRequired;
}

void PlatformPermissions::requestMicrophoneAccess(std::function<void(bool)> callback) {
    if (callback) callback(true);
}

void PlatformPermissions::openMicrophoneSettings() {}

PlatformPermissions::Status PlatformPermissions::accessibilityStatus() {
    return Status::NotRequired;
}

bool PlatformPermissions::requestAccessibility(bool) {
    return true;
}

void PlatformPermissions::openAccessibilitySettings() {}
