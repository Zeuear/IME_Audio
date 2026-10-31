#include "services/InputInjector.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QImage>
#include <QThread>

// 剪贴板写入是各平台通用的，只有"发 Ctrl+V"依赖 platform/<os>/ 的 sendCtrlV()

bool InputInjector::pasteText(const QString& text) {
    return pasteViaClipboard(text);
}

bool InputInjector::pasteImage(const QImage& image) {
    if (image.isNull()) return false;
    QGuiApplication::clipboard()->setImage(image);
    QThread::msleep(30);   // 等剪贴板内容对其它进程可见
    return sendCtrlV();
}

bool InputInjector::pasteFile(const QString& path) {
    if (path.isEmpty() || !setClipboardFile(path)) return false;
    QThread::msleep(30);
    return sendCtrlV();
}
