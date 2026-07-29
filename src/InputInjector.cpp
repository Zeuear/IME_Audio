#include "InputInjector.h"
#include "utils/Logger.h"
#include <QVector>
#include <QClipboard>
#include <QApplication>
#include <QThread>
#include <QMimeData>

#ifdef Q_OS_WIN32
#define NOMINMAX
#include <windows.h>

bool InputInjector::sendCtrlV() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return false;

    DWORD threadId = GetWindowThreadProcessId(hwnd, NULL);
    GUITHREADINFO gti = { sizeof(GUITHREADINFO) };
    if (GetGUIThreadInfo(threadId, &gti) && gti.hwndFocus) {
        hwnd = gti.hwndFocus;
    }

    PostMessage(hwnd, WM_PASTE, 0, 0);
    return true;
}

bool InputInjector::pasteViaClipboard(const QString& text) {
    if (text.isEmpty()) return false;

    QClipboard* clipboard = QApplication::clipboard();
    QString originalText = clipboard->text();
    const QMimeData* originalMime = clipboard->mimeData();
    QString restoreText = originalMime ? originalMime->text() : originalText;

    clipboard->setText(text);
    QThread::msleep(30);
    bool ok = sendCtrlV();

    //QThread::msleep(50);
    //clipboard->setText(restoreText);
    return ok;
}

bool InputInjector::pasteViaUnicodeTyping(const QString& text) {
    if (text.isEmpty()) return false;
    std::wstring wide = text.toStdWString();

    const int kBatchSize = 8;     
    const int kBatchDelayMs = 6;

    size_t i = 0;
    while (i < wide.size()) {
        size_t batchEnd = std::min(wide.size(), i + kBatchSize);
        QVector<INPUT> inputs(static_cast<int>((batchEnd - i) * 2));

        for (size_t j = i; j < batchEnd; ++j) {
            wchar_t wch = wide[j];
            int idx = static_cast<int>((j - i) * 2);

            INPUT& down = inputs[idx];
            down = {};
            down.type = INPUT_KEYBOARD;
            down.ki.wScan = wch;
            down.ki.dwFlags = KEYEVENTF_UNICODE;

            INPUT& up = inputs[idx + 1];
            up = {};
            up.type = INPUT_KEYBOARD;
            up.ki.wScan = wch;
            up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        }

        UINT expected = static_cast<UINT>(inputs.size());
        UINT sent = SendInput(expected, inputs.data(), sizeof(INPUT));
        if (sent != expected) {
            // 常见原因:目标窗口权限高于本进程(UAC/管理员),被 UIPI 拦截
            LOG_ERROR(QString("pasteViaUnicodeTyping partial failure at batch [%1,%2), sent=%3/%4, GetLastError=%5")
                .arg(i).arg(batchEnd).arg(sent).arg(expected).arg(GetLastError()));
            return false; // 部分失败,不再继续发送,避免后续错位
        }

        i = batchEnd;
        if (i < wide.size()) {
            QThread::msleep(kBatchDelayMs);
        }
    }
    return true;
}

static wchar_t* utf8_to_wide(const char* utf8_text) {
    int needed = 0;
    wchar_t* wide_text = NULL;

    if (!utf8_text) {
        return NULL;
    }

    needed = MultiByteToWideChar(CP_UTF8, 0, utf8_text, -1, NULL, 0);
    if (needed <= 0) {
        return NULL;
    }

    wide_text = (wchar_t*)malloc((size_t)needed * sizeof(wchar_t));
    if (!wide_text) {
        return NULL;
    }

    if (MultiByteToWideChar(CP_UTF8, 0, utf8_text, -1, wide_text, needed) <= 0) {
        free(wide_text);
        return NULL;
    }

    return wide_text;
}

BOOL injector_paste_utf8(const char* utf8_text) {
    wchar_t* wide_text = NULL;
    size_t len = 0;
    INPUT* inputs = NULL;
    size_t i = 0;
    UINT sent = 0;

    if (!utf8_text || utf8_text[0] == '\0') {
        return FALSE;
    }

    wide_text = utf8_to_wide(utf8_text);
    if (!wide_text) {
        return FALSE;
    }

    len = wcslen(wide_text);
    if (len == 0) {
        free(wide_text);
        return FALSE;
    }

    inputs = (INPUT*)calloc(len * 2, sizeof(INPUT));
    if (!inputs) {
        free(wide_text);
        return FALSE;
    }

    for (i = 0; i < len; ++i) {
        wchar_t wch = wide_text[i];

        // Key down
        inputs[i * 2].type = INPUT_KEYBOARD;
        inputs[i * 2].ki.wVk = 0;
        inputs[i * 2].ki.wScan = wch;
        inputs[i * 2].ki.dwFlags = KEYEVENTF_UNICODE;

        // Key up
        inputs[i * 2 + 1].type = INPUT_KEYBOARD;
        inputs[i * 2 + 1].ki.wVk = 0;
        inputs[i * 2 + 1].ki.wScan = wch;
        inputs[i * 2 + 1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    }

    sent = SendInput((UINT)(len * 2), inputs, sizeof(INPUT));

    free(inputs);
    free(wide_text);

    return sent == (UINT)(len * 2);
}

bool InputInjector::inject(const QString& text, Mode mode) {
    if (text.isEmpty()) return false;

    switch (mode) {
    case Mode::ClipboardOnly:
        return pasteViaClipboard(text);
    case Mode::UnicodeTypeOnly:
        return injector_paste_utf8(text.toStdString().c_str());
    case Mode::PreferClipboard:
    default:
        if (pasteViaClipboard(text)) return true;
        LOG_ERROR("Clipboard paste failed, falling back to unicode typing.");
        return pasteViaUnicodeTyping(text);
    }
}

#else

bool InputInjector::sendCtrlV() { return false; }
bool InputInjector::pasteViaClipboard(const QString&) { return false; }
bool InputInjector::pasteViaUnicodeTyping(const QString&) { return false; }
bool InputInjector::inject(const QString&, Mode) {
    // 非 Windows 平台可接入 xdotool / AppleScript 等实现
    return false;
}

#endif