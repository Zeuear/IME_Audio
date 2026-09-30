#pragma once
#include <QString>

class InputInjector {
public:
    enum class Mode {
        PreferClipboard,   
        ClipboardOnly,
        UnicodeTypeOnly
    };
    // 各平台最可靠的注入方式由 platform/<os>/ 决定，调用方不应自己按系统挑选。
    static Mode defaultMode();
    static bool inject(const QString& text, Mode mode);

private:
    static bool sendCtrlV();
    static bool pasteViaClipboard(const QString& text);
    static bool pasteViaUnicodeTyping(const QString& text);
};