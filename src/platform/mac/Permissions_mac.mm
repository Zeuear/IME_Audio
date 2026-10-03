#include "../../utils/PlatformPermissions.h"
#include "../../utils/Logger.h"

#import <AVFoundation/AVFoundation.h>
#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>

#include <QCoreApplication>
#include <QProcess>
#include <QTimer>

#include <memory>

namespace {

constexpr int kPermissionPollMs = 1000;

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

// 麦克风与辅助功能授权都要重启进程才真正生效，所以授权完成后自动重启一次。
void relaunchSelf() {
    const QString bundle = QString::fromNSString([[NSBundle mainBundle] bundlePath]);
    const bool isBundle = bundle.endsWith(".app");
    const QString target = isBundle ? bundle : QCoreApplication::applicationFilePath();
    const qint64 pid = QCoreApplication::applicationPid();
    // 必须等本进程退出、释放单实例锁后再启动，否则新实例会把自己当成第二个实例直接退出。
    const QString launch = isBundle ? QString("open \"$0\"") : QString("\"$0\"");
    const QString script = QString("while kill -0 %1 2>/dev/null; do sleep 0.2; done; %2").arg(pid).arg(launch);

    LOG_INFO(QString("Permissions(mac): permissions granted, relaunching %1").arg(target));
    if (!QProcess::startDetached("/bin/sh", { "-c", script, target })) {
        LOG_ERROR("Permissions(mac): failed to spawn relaunch helper, please restart manually");
        return;
    }
    QCoreApplication::exit(0);
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
    // 麦克风（录音）和辅助功能（文本注入）都是硬性前提。启动时就引导授权，
    // 而不是等第一次录音或第一句话转录完才失败。
    if (microphoneStatus() == Status::Granted && accessibilityStatus() == Status::Granted) {
        return;
    }
    LOG_INFO(QString("Permissions(mac): startup permissions missing, microphone=%1 accessibility=%2")
                 .arg(static_cast<int>(microphoneStatus())).arg(static_cast<int>(accessibilityStatus())));

    // 授权在系统设置/系统弹框里异步完成，没有回调可等，只能轮询。
    // 先麦克风后辅助功能，避免两个系统弹框叠在一起。
    struct Guide { bool micGuided = false; bool axGuided = false; };
    auto guide = std::make_shared<Guide>();
    auto* timer = new QTimer(qApp);
    timer->setInterval(kPermissionPollMs);

    auto tick = [guide, timer]() {
        const Status mic = microphoneStatus();
        const Status ax = accessibilityStatus();
        if (mic == Status::Granted && ax == Status::Granted) {
            timer->stop();
            timer->deleteLater();
            relaunchSelf();
            return;
        }

        if (mic != Status::Granted && !guide->micGuided) {
            guide->micGuided = true;
            if (mic == Status::NotDetermined) {
                requestMicrophoneAccess({});
            } else {
                openMicrophoneSettings();
            }
        }

        // 麦克风弹框还没答复时先不弹辅助功能，答复（无论允许与否）后再继续。
        if (ax != Status::Granted && !guide->axGuided && mic != Status::NotDetermined) {
            guide->axGuided = true;
            requestAccessibility(true);
        }
    };

    QObject::connect(timer, &QTimer::timeout, tick);
    tick();
    timer->start();
}
