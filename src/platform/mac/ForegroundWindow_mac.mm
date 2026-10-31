#include "services/ForegroundWindow.h"

#import <AppKit/AppKit.h>

namespace ForegroundWindow {

// macOS 没有跨应用的窗口句柄概念，记住前台应用的 pid，还焦点时激活该应用
Handle current() {
    NSRunningApplication* app = [[NSWorkspace sharedWorkspace] frontmostApplication];
    return app ? static_cast<Handle>(app.processIdentifier) : 0;
}

bool restore(Handle handle) {
    if (!handle) return false;
    NSRunningApplication* app = [NSRunningApplication runningApplicationWithProcessIdentifier:static_cast<pid_t>(handle)];
    if (!app) return false;
    return [app activateWithOptions:NSApplicationActivateIgnoringOtherApps];
}

bool bringToFront(quintptr) {
    [NSApp activateIgnoringOtherApps:YES];
    return true;
}

bool isMouseButtonDown() {
    return ([NSEvent pressedMouseButtons] & 0x3) != 0;   // bit0 左键，bit1 右键
}

}
