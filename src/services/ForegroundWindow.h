#pragma once
#include <QtGlobal>

// 全局热键呼出搜索框时，要先记住用户正在输入的窗口，选完再把焦点还回去，粘贴才会落到那里。
// 各平台的实现在 platform/<os>/ForegroundWindow_<os>.*，调用方不要自己按系统区分。
namespace ForegroundWindow {

// Windows 是 HWND，macOS 是前台应用的 pid，Linux 暂未实现
using Handle = quintptr;

// 当前前台窗口；取不到返回 0
Handle current();

// 把焦点还给 handle 对应的窗口；平台不支持或失败返回 false
bool restore(Handle handle);

// 把本应用刚显示的窗口（winId）提到前台并拿到键盘焦点。全局热键触发时本应用在后台，
// 光 activateWindow 常被系统的前台锁定拦下
bool bringToFront(quintptr winId);

// 鼠标左/右键此刻是否按着（全局，不限本应用窗口）。不拿焦点的弹窗收不到点击事件，
// 靠轮询它判断用户是否点了弹窗外面；平台不支持时恒为 false
bool isMouseButtonDown();

}
