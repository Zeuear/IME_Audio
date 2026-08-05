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
#include <imm.h>

/*
#include <uiautomation.h>
#include <atlbase.h>
#include <atlcomcli.h>
#include <string>
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "ole32.lib")


BOOL uia_probe_value_pattern(CComPtr<IUIAutomationValuePattern>& out_value_pattern,
                             CComPtr<IUIAutomationElement>& focused) {
    CComPtr<IUIAutomation> automation;
    HRESULT hr;

    hr = automation.CoCreateInstance(CLSID_CUIAutomation);
    if (FAILED(hr) || !automation) return FALSE;

    hr = automation->GetFocusedElement(&focused);
    if (FAILED(hr) || !focused) return FALSE;

    VARIANT_BOOL is_available = VARIANT_FALSE;
    hr = focused->GetCurrentPropertyValue(UIA_IsValuePatternAvailablePropertyId, nullptr);

    hr = focused->GetCurrentPatternAs(
        UIA_ValuePatternId, IID_PPV_ARGS(&out_value_pattern));

    if (FAILED(hr) || !out_value_pattern) {
        return FALSE;
    }

    CComBSTR test_read;
    hr = out_value_pattern->get_CurrentValue(&test_read);
    if (FAILED(hr)) {
        out_value_pattern.Release();
        return FALSE;
    }
    return TRUE;
}


BOOL injector_append_text_via_uia(CComPtr<IUIAutomationValuePattern>& value_pattern,
                                  CComPtr<IUIAutomationElement>& focused,
                                  const wchar_t* wide_text)
{
    HRESULT hr;
    CComBSTR current_bstr;
    hr = value_pattern->get_CurrentValue(&current_bstr);
    if (FAILED(hr)) {
        return FALSE;
    }

    std::wstring combined;
    if (current_bstr.Length() > 0) {
        combined.assign(current_bstr, current_bstr.Length());
    }
    combined.append(wide_text);

    // ---- 第二步：整体写回（旧内容 + 新内容）----
    CComBSTR bstr_combined(combined.c_str());
    hr = value_pattern->SetValue(bstr_combined);
    if (FAILED(hr)) {
        return FALSE;
    }

    // ---- 第三步：把光标/选区移动到文本末尾 ----
    CComPtr<IUIAutomationTextPattern> text_pattern;
    hr = focused->GetCurrentPatternAs(
        UIA_TextPatternId, IID_PPV_ARGS(&text_pattern));

    if (SUCCEEDED(hr) && text_pattern) {
        CComPtr<IUIAutomationTextRange> doc_range;
        hr = text_pattern->get_DocumentRange(&doc_range);
        if (SUCCEEDED(hr) && doc_range) {

            hr = doc_range->MoveEndpointByRange(
                TextPatternRangeEndpoint_Start,
                doc_range,
                TextPatternRangeEndpoint_End);

            if (SUCCEEDED(hr)) {
                doc_range->Select();
            }
        }
    }
    return TRUE;
}

*/

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

    const int kBatchSize = 20;     
    const int kBatchDelayMs = 1;

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
            LOG_WARN(QString("pasteViaUnicodeTyping partial failure at batch [%1,%2), sent=%3/%4, GetLastError=%5")
                .arg(i).arg(batchEnd).arg(sent).arg(expected).arg(GetLastError()));
            return false;
        }

        i = batchEnd;
        if (i < wide.size()) {  
            QThread::msleep(kBatchDelayMs);
        }
    }
    return true;
}

#define INJECT_CHAR_DELAY_MS 10

typedef struct {
    HWND hwnd;
    HIMC himc;
    BOOL had_context;
    BOOL prev_open_status;
} ImeGuard;

static void ime_guard_disable(ImeGuard* guard) {
    guard->hwnd = GetForegroundWindow();
    guard->himc = NULL;
    guard->had_context = FALSE;
    guard->prev_open_status = FALSE;

    if (!guard->hwnd) {
        return;
    }

    guard->himc = ImmGetContext(guard->hwnd);
    if (guard->himc) {
        guard->had_context = TRUE;
        guard->prev_open_status = ImmGetOpenStatus(guard->himc);
        if (guard->prev_open_status) {
            ImmSetOpenStatus(guard->himc, FALSE);
        }
    }
}

static void ime_guard_restore(ImeGuard* guard) {
    if (guard->had_context && guard->himc) {
        ImmSetOpenStatus(guard->himc, guard->prev_open_status);
        ImmReleaseContext(guard->hwnd, guard->himc);
    }
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
    ImeGuard guard = { 0 };
    BOOL all_ok = TRUE;
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

    if (!GetForegroundWindow()) {
        free(wide_text);
        return FALSE;
    }

    // 关闭 IME，防止中文输入法把英文字符劫持成拼音候选
    ime_guard_disable(&guard);
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
    ime_guard_restore(&guard);

    free(wide_text);
    return all_ok;
}


bool InputInjector::inject(const QString& text, Mode mode) {
    if (text.isEmpty()) return false;
        
    //CComPtr<IUIAutomationElement> focused;
    //CComPtr<IUIAutomationValuePattern> value_pattern;
    //BOOL result = uia_probe_value_pattern(value_pattern, focused);
    //if (result) {
    //    return injector_append_text_via_uia(value_pattern, focused, text.toStdWString().c_str());
    //}

    LOG_DEBUG("sendText: falling back to SendInput unicode typing");
    return injector_paste_utf8(text.toUtf8().constData());


    //switch (mode) {
    //case Mode::ClipboardOnly:
    //    return pasteViaClipboard(text);
    //case Mode::UnicodeTypeOnly:
    //    return injector_paste_utf8(text.toUtf8().constData());
    //    //return injector_append_text_via_uia(text.toStdWString().c_str());
    //case Mode::PreferClipboard:
    //default:
    //    if (pasteViaClipboard(text)) return true;
    //    LOG_WARN("Clipboard paste failed, falling back to unicode typing.");
    //    return pasteViaUnicodeTyping(text);
    //}
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