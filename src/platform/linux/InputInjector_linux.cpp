#include "services/InputInjector.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>
#include <QUrl>

// Linux 文本注入尚未实现（X11 下可走 XTestFakeKeyEvent，Wayland 需另寻方案）。
InputInjector::Mode InputInjector::defaultMode() { return Mode::UnicodeTypeOnly; }
bool InputInjector::sendCtrlV() { return false; }
bool InputInjector::setClipboardFile(const QString& path) {
    auto* mime = new QMimeData;   // 所有权交给剪贴板
    mime->setUrls({ QUrl::fromLocalFile(path) });
    QGuiApplication::clipboard()->setMimeData(mime);
    return true;
}
bool InputInjector::pasteViaClipboard(const QString&) { return false; }
bool InputInjector::pasteViaUnicodeTyping(const QString&) { return false; }
bool InputInjector::inject(const QString&, Mode) { return false; }
