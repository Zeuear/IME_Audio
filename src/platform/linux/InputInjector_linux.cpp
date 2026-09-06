#include "../../InputInjector.h"

// Linux 文本注入尚未实现（X11 下可走 XTestFakeKeyEvent，Wayland 需另寻方案）。
bool InputInjector::sendCtrlV() { return false; }
bool InputInjector::pasteViaClipboard(const QString&) { return false; }
bool InputInjector::pasteViaUnicodeTyping(const QString&) { return false; }
bool InputInjector::inject(const QString&, Mode) { return false; }
