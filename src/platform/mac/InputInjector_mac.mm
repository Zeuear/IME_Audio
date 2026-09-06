#include "../../InputInjector.h"

// macOS 文本注入尚未实现。
//
// 计划中的实现是 CGEventCreateKeyboardEvent + CGEventKeyboardSetUnicodeString（短文本）
// 与剪贴板 + 合成 ⌘V（长文本），两者都需要用户在「系统设置 → 隐私与安全性 → 辅助功能」
// 中授权本应用。这部分必须在真机上验证 TCC 授权流程后才能落地，无法盲写。
bool InputInjector::sendCtrlV() { return false; }
bool InputInjector::pasteViaClipboard(const QString&) { return false; }
bool InputInjector::pasteViaUnicodeTyping(const QString&) { return false; }
bool InputInjector::inject(const QString&, Mode) { return false; }
