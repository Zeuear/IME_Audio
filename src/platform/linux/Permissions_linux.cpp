#include "../../utils/PlatformPermissions.h"

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
