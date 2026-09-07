#include "../../utils/PlatformPermissions.h"

#import <AVFoundation/AVFoundation.h>
#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>

#include <memory>

namespace {

// 系统设置 → 隐私与安全性 的深链。anchor 是各子面板的固定标识符，
// macOS 13 起的「系统设置」和更早的「系统偏好设置」都认这个 scheme。
void openPrivacyPane(NSString* anchor) {
    NSString* spec = [NSString
        stringWithFormat:@"x-apple.systempreferences:com.apple.preference.security?%@", anchor];
    NSURL* url = [NSURL URLWithString:spec];
    if (url) {
        [[NSWorkspace sharedWorkspace] openURL:url];
    }
}

} // namespace

PlatformPermissions::Status PlatformPermissions::microphoneStatus() {
    switch ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio]) {
    case AVAuthorizationStatusAuthorized:
        return Status::Granted;
    case AVAuthorizationStatusNotDetermined:
        return Status::NotDetermined;
    // Restricted（家长控制 / MDM 限制）对用户而言和 Denied 是同一件事：
    // 系统不会再弹框，本应用也无法自行改变，都只能引导到系统设置。
    case AVAuthorizationStatusRestricted:
    case AVAuthorizationStatusDenied:
    default:
        return Status::Denied;
    }
}

void PlatformPermissions::requestMicrophoneAccess(std::function<void(bool)> callback) {
    // block 会逃逸到系统的授权流程里，std::function 必须堆分配后按值捕获。
    auto shared = std::make_shared<std::function<void(bool)>>(std::move(callback));
    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio
                             completionHandler:^(BOOL granted) {
                                 if (*shared) {
                                     (*shared)(granted == YES);
                                 }
                             }];
}

void PlatformPermissions::openMicrophoneSettings() {
    openPrivacyPane(@"Privacy_Microphone");
}

PlatformPermissions::Status PlatformPermissions::accessibilityStatus() {
    // 辅助功能没有 NotDetermined 这一档：AXIsProcessTrusted 只回答"现在能不能用"。
    return AXIsProcessTrusted() ? Status::Granted : Status::Denied;
}

bool PlatformPermissions::requestAccessibility(bool prompt) {
    NSDictionary* options = @{ (__bridge id)kAXTrustedCheckOptionPrompt : @(prompt) };
    return AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)options) == TRUE;
}

void PlatformPermissions::openAccessibilitySettings() {
    openPrivacyPane(@"Privacy_Accessibility");
}
