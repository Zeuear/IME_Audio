#include "services/ForegroundWindow.h"

#define NOMINMAX
#include <windows.h>

namespace ForegroundWindow {

Handle current() {
    return reinterpret_cast<Handle>(::GetForegroundWindow());
}

bool restore(Handle handle) {
    HWND hwnd = reinterpret_cast<HWND>(handle);
    if (!hwnd || !::IsWindow(hwnd)) return false;
    HWND fg = ::GetForegroundWindow();
    if (fg == hwnd) return true;

    // 系统不允许后台进程直接抢前台，借用前台线程和目标线程的输入队列绕过限制
    DWORD fgTid = fg ? ::GetWindowThreadProcessId(fg, nullptr) : 0;
    DWORD targetTid = ::GetWindowThreadProcessId(hwnd, nullptr);
    DWORD curTid = ::GetCurrentThreadId();

    bool attachedFg = false, attachedTarget = false;
    if (fgTid && fgTid != curTid)
        attachedFg = ::AttachThreadInput(curTid, fgTid, TRUE) != FALSE;
    if (targetTid != curTid && targetTid != fgTid)
        attachedTarget = ::AttachThreadInput(curTid, targetTid, TRUE) != FALSE;

    if (::IsIconic(hwnd)) ::ShowWindow(hwnd, SW_RESTORE);
    BOOL ok = ::SetForegroundWindow(hwnd);
    ::SetFocus(hwnd);

    if (attachedFg) ::AttachThreadInput(curTid, fgTid, FALSE);
    if (attachedTarget) ::AttachThreadInput(curTid, targetTid, FALSE);

    if (!ok) {
        // 按一下 Alt 可解除前台锁定，再试一次
        ::keybd_event(VK_MENU, 0, 0, 0);
        ::keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
        ok = ::SetForegroundWindow(hwnd);
    }
    return ok != FALSE;
}

bool bringToFront(quintptr winId) {
    return restore(static_cast<Handle>(winId));
}

}
