#pragma once
#include <QString>

class InputInjector {
public:
    enum class Mode {
        PreferClipboard,   
        ClipboardOnly,
        UnicodeTypeOnly
    };

    static bool inject(const QString& text, Mode mode = Mode::PreferClipboard);

private:
    static bool sendCtrlV();
    static bool pasteViaClipboard(const QString& text);
    static bool pasteViaUnicodeTyping(const QString& text);
};