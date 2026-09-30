#include "../../utils/PlatformPermissions.h"
#include "../../utils/Logger.h"

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
        const BOOL opened = [[NSWorkspace sharedWorkspace] openURL:url];
        LOG_DEBUG(QString("Permissions(mac): open privacy pane %1 -> %2")
                      .arg(QString::fromNSString(anchor)).arg(opened == YES));
    } else {
        LOG_WARN(QString("Permissions(mac): invalid privacy pane url for %1").arg(QString::fromNSString(anchor)));
    }
}

} // namespace

PlatformPermissions::Status PlatformPermissions::microphoneStatus() {
    const AVAuthorizationStatus raw = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
    // 原始值 0=NotDetermined 1=Restricted 2=Denied 3=Authorized；Restricted 与 Denied 在下面被合并，
    // 排查"为什么弹不出授权框"时需要区分。
    if (raw == AVAuthorizationStatusRestricted) {
        LOG_WARN("Permissions(mac): 麦克风授权状态为 Restricted（家长控制/MDM 限制）");
    }
    switch (raw) {
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
    LOG_DEBUG("Permissions(mac): requesting microphone access");
    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio
                             completionHandler:^(BOOL granted) {
                                 LOG_DEBUG(QString("Permissions(mac): microphone request completed, granted=%1").arg(granted == YES));
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
    const bool trusted = AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)options) == TRUE;
    LOG_DEBUG(QString("Permissions(mac): accessibility trusted=%1 (prompt=%2)，"
                      "若已在系统设置中勾选仍为 false，请删除旧条目后重新添加并重启应用（重新签名会使授权失效）")
                  .arg(trusted).arg(prompt));
    return trusted;
}

void PlatformPermissions::openAccessibilitySettings() {
    openPrivacyPane(@"Privacy_Accessibility");
}

void PlatformPermissions::requestStartupPermissions() {
    // 辅助功能授权是文本注入的硬性前提。启动时就引导授权，而不是等第一句话转录完、
    // 发现输不进去才提示。
    requestAccessibility(true);
}
