#include "utils/PlatformPermissions.h"

// Windows 上麦克风与模拟输入都不需要运行时授权：
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

void PlatformPermissions::relaunchApp() {}
