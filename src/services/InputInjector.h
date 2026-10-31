#pragma once
#include <QString>

class QImage;

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
    // 话术库用：多行文本和图片不适合逐字键入，放进剪贴板后向前台窗口发 Ctrl+V
    static bool pasteText(const QString& text);
    static bool pasteImage(const QImage& image);
    static bool pasteFile(const QString& path);

private:
    static bool sendCtrlV();
    static bool pasteViaClipboard(const QString& text);
    static bool pasteViaUnicodeTyping(const QString& text);
};