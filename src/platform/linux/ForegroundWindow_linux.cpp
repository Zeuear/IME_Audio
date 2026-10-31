#include "services/ForegroundWindow.h"

// Linux 尚未实现：X11 需要 _NET_ACTIVE_WINDOW，Wayland 不允许应用查询或切换其它应用的焦点。
// 搜索框关闭后由窗口管理器决定焦点归属。
namespace ForegroundWindow {

Handle current() { return 0; }
bool restore(Handle) { return false; }
bool bringToFront(quintptr) { return false; }
bool isMouseButtonDown() { return false; }

}
