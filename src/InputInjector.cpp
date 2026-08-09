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

#include <uiautomation.h>
#include <atlbase.h>
#include <atlcomcli.h>
#include <string>
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "ole32.lib")

class NotepadUiaInjector {
public:
    static NotepadUiaInjector& instance() {
        thread_local NotepadUiaInjector inst;
        return inst;
    }

    bool tryInject(const wchar_t* wideText) {
        if (!isForegroundNotepad()) {
            return false;
        }

        CComPtr<IUIAutomationElement> focused;
        CComPtr<IUIAutomationValuePattern> valuePattern;
        if (!probeValuePattern(valuePattern, focused)) {
            return false;
        }

        return appendText(valuePattern, focused, wideText);
    }

private:
    NotepadUiaInjector() = default;
    NotepadUiaInjector(const NotepadUiaInjector&) = delete;
    NotepadUiaInjector& operator=(const NotepadUiaInjector&) = delete;

    bool isForegroundNotepad() {
        HWND hwnd = GetForegroundWindow();
        if (!hwnd) {
            return false;
        }

        if (hwnd == m_cachedHwnd) {
            return m_cachedIsNotepad;
        }

        m_cachedHwnd = hwnd;
        m_cachedIsNotepad = queryIsNotepad(hwnd);

        m_cachedFocused.Release();
        m_cachedValuePattern.Release();
        return m_cachedIsNotepad;
    }

    static bool queryIsNotepad(HWND hwnd) {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid == 0) {
            return false;
        }

        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!process) {
            return false;
        }

        wchar_t path[MAX_PATH] = { 0 };
        DWORD size = MAX_PATH;
        BOOL ok = QueryFullProcessImageNameW(process, 0, path, &size);
        CloseHandle(process);
        if (!ok) {
            return false;
        }

        std::wstring fullPath(path);
        size_t pos = fullPath.find_last_of(L"\\/");
        std::wstring filename = (pos == std::wstring::npos) ? fullPath : fullPath.substr(pos + 1);
        std::transform(filename.begin(), filename.end(), filename.begin(), ::towlower);
        return filename == L"notepad.exe";
    }

    bool ensureAutomation() {
        if (m_automation) {
            return true;
        }
        HRESULT hr = m_automation.CoCreateInstance(CLSID_CUIAutomation);
        return SUCCEEDED(hr) && m_automation;
    }

    bool probeValuePattern(CComPtr<IUIAutomationValuePattern>& outValuePattern,
        CComPtr<IUIAutomationElement>& outFocused) {
        if (!ensureAutomation()) {
            return false;
        }

        CComPtr<IUIAutomationElement> focused;
        HRESULT hr = m_automation->GetFocusedElement(&focused);
        if (FAILED(hr) || !focused) {
            return false;
        }

        CComPtr<IUIAutomationValuePattern> valuePattern;
        hr = focused->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&valuePattern));
        if (FAILED(hr) || !valuePattern) {
            return false;
        }

        CComBSTR testRead;
        hr = valuePattern->get_CurrentValue(&testRead);
        if (FAILED(hr)) {
            return false;
        }
        outFocused = focused;
        outValuePattern = valuePattern;
        return true;
    }

    static bool appendText(CComPtr<IUIAutomationValuePattern>& valuePattern,
        CComPtr<IUIAutomationElement>& focused,
        const wchar_t* wideText) {

        CComBSTR currentBstr;
        HRESULT hr = valuePattern->get_CurrentValue(&currentBstr);
        if (FAILED(hr)) {
            return false;
        }
        std::wstring fullText;
        if (currentBstr.Length() > 0) {
            fullText.assign(currentBstr, currentBstr.Length());
        }

        CComPtr<IUIAutomationTextPattern> textPattern;
        hr = focused->GetCurrentPatternAs(UIA_TextPatternId, IID_PPV_ARGS(&textPattern));

        // 拿不到 TextPattern 就退回旧的"追加到末尾"逻辑，保证兼容性
        if (FAILED(hr) || !textPattern) {
            std::wstring combined = fullText + wideText;
            CComBSTR bstrCombined(combined.c_str());
            return SUCCEEDED(valuePattern->SetValue(bstrCombined));
        }

        CComPtr<IUIAutomationTextRange> docRange;
        hr = textPattern->get_DocumentRange(&docRange);
        if (FAILED(hr) || !docRange) {
            std::wstring combined = fullText + wideText;
            CComBSTR bstrCombined(combined.c_str());
            return SUCCEEDED(valuePattern->SetValue(bstrCombined));
        }

        // 默认插入到文末（找不到光标位置时的兜底）
        int selStart = static_cast<int>(fullText.size());
        int selEnd = selStart;

        CComPtr<IUIAutomationTextRangeArray> selection;
        hr = textPattern->GetSelection(&selection);
        if (SUCCEEDED(hr) && selection) {
            int count = 0;
            selection->get_Length(&count);
            if (count > 0) {
                CComPtr<IUIAutomationTextRange> selRange;
                selection->GetElement(0, &selRange);
                if (selRange) {
                    // 通过"文档起点 -> 选区起点"这段文字的长度，反推选区起点的字符偏移
                    CComPtr<IUIAutomationTextRange> preStart;
                    docRange->Clone(&preStart);
                    preStart->MoveEndpointByRange(
                        TextPatternRangeEndpoint_End,
                        selRange, TextPatternRangeEndpoint_Start);
                    CComBSTR preStartText;
                    preStart->GetText(-1, &preStartText);
                    selStart = preStartText.Length();

                    // 同理反推选区终点的偏移（如果只是光标，selEnd == selStart）
                    CComPtr<IUIAutomationTextRange> preEnd;
                    docRange->Clone(&preEnd);
                    preEnd->MoveEndpointByRange(
                        TextPatternRangeEndpoint_End,
                        selRange, TextPatternRangeEndpoint_End);
                    CComBSTR preEndText;
                    preEnd->GetText(-1, &preEndText);
                    selEnd = preEndText.Length();
                }
            }
        }

        // 边界保护，避免偏移超出实际文本长度
        const int textLen = static_cast<int>(fullText.size());
        selStart = std::clamp(selStart, 0, textLen);
        selEnd = std::clamp(selEnd, selStart, textLen);

        // 拼接：光标前 + 新插入文本 + （如有选区则跳过被选中的部分）光标后
        std::wstring combined;
        combined.reserve(fullText.size() + wcslen(wideText));
        combined.append(fullText, 0, selStart);
        combined.append(wideText);
        combined.append(fullText, selEnd, std::wstring::npos);

        CComBSTR bstrCombined(combined.c_str());
        hr = valuePattern->SetValue(bstrCombined);
        if (FAILED(hr)) {
            return false;
        }

        const int caretPos = selStart + static_cast<int>(wcslen(wideText));
        CComPtr<IUIAutomationTextRange> newDocRange;
        hr = textPattern->get_DocumentRange(&newDocRange);
        if (SUCCEEDED(hr) && newDocRange) {
            CComPtr<IUIAutomationTextRange> caretRange;
            newDocRange->Clone(&caretRange);

            caretRange->MoveEndpointByRange(
                TextPatternRangeEndpoint_End,
                caretRange, TextPatternRangeEndpoint_Start);

            int moved = 0;
            caretRange->MoveEndpointByUnit(
                TextPatternRangeEndpoint_Start, TextUnit_Character, caretPos, &moved);

            caretRange->MoveEndpointByRange(
                TextPatternRangeEndpoint_End,
                caretRange, TextPatternRangeEndpoint_Start);

            caretRange->Select();
        }

        return true;
    }
private:
    CComPtr<IUIAutomation> m_automation;

    HWND m_cachedHwnd = nullptr;
    bool m_cachedIsNotepad = false;

    CComPtr<IUIAutomationElement> m_cachedFocused;
    CComPtr<IUIAutomationValuePattern> m_cachedValuePattern;
};




class ImeGuard {
public:
    explicit ImeGuard(HWND hwnd) : m_hwnd(hwnd) {
        if (!m_hwnd) {
            return;
        }
        m_himc = ImmGetContext(m_hwnd);
        if (m_himc) {
            m_hadContext = TRUE;
            m_prevOpenStatus = ImmGetOpenStatus(m_himc);
            if (m_prevOpenStatus) {
                ImmSetOpenStatus(m_himc, FALSE);
            }
        }
    }

    ~ImeGuard() {
        if (m_hadContext && m_himc) {
            ImmSetOpenStatus(m_himc, m_prevOpenStatus);
            ImmReleaseContext(m_hwnd, m_himc);
        }
    }

    ImeGuard(const ImeGuard&) = delete;
    ImeGuard& operator=(const ImeGuard&) = delete;

private:
    HWND m_hwnd = nullptr;
    HIMC m_himc = nullptr;
    BOOL m_hadContext = FALSE;
    BOOL m_prevOpenStatus = FALSE;
};


class UnicodeTextInjector {
public:
    struct Options {
        int batchSize = 20;     // 每批发送的字符数，避免单次 SendInput 数组过大丢字符
        int batchDelayMs = 1;   // 批次之间的间隔，给目标应用消息队列喘息时间
        bool guardIme = true;   // 是否在注入期间临时关闭输入法，防止英文字符被劫持成拼音候选
    };

    explicit UnicodeTextInjector(Options options = {}) : m_options(options) {}

    bool inject(const QString& text) const {
        if (text.isEmpty()) {
            return false;
        }

        HWND hwnd = GetForegroundWindow();
        if (!hwnd) {
            LOG_ERROR("UnicodeTextInjector: no foreground window");
            return false;
        }

        std::wstring wide = text.toStdWString();
        if (wide.empty()) {
            return false;
        }

        std::unique_ptr<ImeGuard> imeGuard;
        if (m_options.guardIme) {
            imeGuard = std::make_unique<ImeGuard>(hwnd);
        }
        return sendInBatches(wide);
    }

private:
    bool sendInBatches(const std::wstring& wide) const {
        const size_t batchSize = static_cast<size_t>(std::max(1, m_options.batchSize));
        size_t i = 0;

        while (i < wide.size()) {
            size_t batchEnd = std::min(wide.size(), i + batchSize);
            if (!sendBatch(wide, i, batchEnd)) {
                return false;
            }
            i = batchEnd;
            if (i < wide.size() && m_options.batchDelayMs > 0) {
                QThread::msleep(static_cast<unsigned long>(m_options.batchDelayMs));
            }
        }
        return true;
    }

    static bool sendBatch(const std::wstring& wide, size_t begin, size_t end) {
        std::vector<INPUT> inputs((end - begin) * 2);

        for (size_t j = begin; j < end; ++j) {
            wchar_t wch = wide[j];
            size_t idx = (j - begin) * 2;

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
            LOG_WARN(QString("UnicodeTextInjector: partial send at [%1,%2), sent=%3/%4, GetLastError=%5")
                .arg(begin).arg(end).arg(sent).arg(expected).arg(GetLastError()));
            return false;
        }
        return true;
    }

private:
    Options m_options;
};


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

bool InputInjector::inject(const QString& text, Mode mode) {
    if (text.isEmpty()) return false;

    std::wstring wide = text.toStdWString();
    if (NotepadUiaInjector::instance().tryInject(wide.c_str())) {
        return true;
    }

    LOG_DEBUG("sendText: falling back to SendInput unicode typing");
    static const UnicodeTextInjector injector;
    return injector.inject(text);
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