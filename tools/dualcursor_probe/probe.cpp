// 双光标 P0 验证原型：独立小程序，不属于主程序构建。
// 目的：在真实目标（Google 文档、Word、记事本、浏览器输入框……）上量化两条写入通道：
//   edit   = A 档：向目标焦点控件发 EM_* 消息，不切焦点（只对经典 Edit / RichEdit 有效）
//   switch = D 档：等键盘空闲 → 切到目标 → SendInput 写入 → 切回；切换期间用低级键盘钩子
//            缓存用户按键，切回后按原顺序重放
//
// 用法：probe.exe <edit|switch> [--count N] [--interval ms] [--idle ms] [--settle ms]
//                 [--wait ms] [--text 文本]
// 流程：先切到目标窗口并点进正文 → 倒计时后记录为目标 → 再切到另一个窗口 B 持续打字 →
//       程序每隔 interval 往目标写一段带序号的文字，并打印耗时与结果。
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cwchar>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {

// ---------- 小工具 ----------
std::string u8(const std::wstring& w)
{
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

void logf(const char* fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    std::printf("%s\n", buf);
    std::fflush(stdout);
}

double nowMs()
{
    static LARGE_INTEGER freq = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f; }();
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return c.QuadPart * 1000.0 / freq.QuadPart;
}

DWORD idleMs()
{
    LASTINPUTINFO li{ sizeof(li) };
    GetLastInputInfo(&li);
    return GetTickCount() - li.dwTime;
}

HWND rootOf(HWND h) { return h ? GetAncestor(h, GA_ROOT) : nullptr; }

std::wstring titleOf(HWND h)
{
    wchar_t t[256] = {};
    GetWindowTextW(h, t, 256);
    return t;
}

std::wstring classOf(HWND h)
{
    wchar_t c[128] = {};
    GetClassNameW(h, c, 128);
    return c;
}

HWND focusChildOf(HWND root)
{
    GUITHREADINFO gti{ sizeof(gti) };
    DWORD tid = GetWindowThreadProcessId(root, nullptr);
    if (tid && GetGUIThreadInfo(tid, &gti) && gti.hwndFocus) return gti.hwndFocus;
    return root;
}

// ---------- 低级键盘钩子：切换期间缓存用户按键 ----------
struct KeyRec {
    DWORD vk, scan, flags;
    bool up;
};

std::mutex g_mu;
std::vector<KeyRec> g_buf;
std::set<DWORD> g_swallowedDown;   // 已被吞掉按下的键：它的抬起也要吞，否则目标收到孤零零的 KeyUp
std::atomic<bool> g_swallow{ false };
std::atomic<int> g_swallowedCount{ 0 };

LRESULT CALLBACK lowLevelKeyboard(int code, WPARAM wp, LPARAM lp)
{
    if (code == HC_ACTION) {
        auto* k = reinterpret_cast<KBDLLHOOKSTRUCT*>(lp);
        // 自己 SendInput 出来的事件带 INJECTED 标记，必须放行，否则注入的文字也被吞
        if (!(k->flags & LLKHF_INJECTED) && g_swallow.load()) {
            const bool down = (wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN);
            std::lock_guard<std::mutex> lk(g_mu);
            if (down) {
                g_swallowedDown.insert(k->vkCode);
                g_buf.push_back({ k->vkCode, k->scanCode, k->flags, false });
                ++g_swallowedCount;
                return 1;
            }
            if (g_swallowedDown.count(k->vkCode)) {
                g_buf.push_back({ k->vkCode, k->scanCode, k->flags, true });
                return 1;
            }
            // 切换前就按着的键：它的抬起放行，否则原窗口里键会一直按住
        }
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}

void hookThreadMain()
{
    HHOOK h = SetWindowsHookExW(WH_KEYBOARD_LL, lowLevelKeyboard, GetModuleHandleW(nullptr), 0);
    if (!h) {
        logf("[warn] 安装键盘钩子失败 err=%lu，D 档不会缓存用户按键", GetLastError());
        return;
    }
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    UnhookWindowsHookEx(h);
}

void replayBuffered(int& replayed)
{
    // 重放期间钩子仍在吞用户的新按键并追加进 g_buf，所以循环到缓冲清空再关闭吞键
    for (;;) {
        std::vector<KeyRec> local;
        {
            std::lock_guard<std::mutex> lk(g_mu);
            if (g_buf.empty()) {
                g_swallow = false;
                g_swallowedDown.clear();
                return;
            }
            local.swap(g_buf);
        }
        for (const KeyRec& r : local) {
            INPUT in{};
            in.type = INPUT_KEYBOARD;
            in.ki.wVk = (WORD)r.vk;
            in.ki.wScan = (WORD)r.scan;
            in.ki.dwFlags = (r.up ? KEYEVENTF_KEYUP : 0) | ((r.flags & LLKHF_EXTENDED) ? KEYEVENTF_EXTENDEDKEY : 0);
            SendInput(1, &in, sizeof(in));
            ++replayed;
        }
    }
}

// ---------- 切焦点 ----------
// 返回 0 失败；1 = AttachThreadInput 成功；2 = 靠模拟 Alt 键兜底成功
int forceFocus(HWND root)
{
    if (!root || !IsWindow(root)) return 0;
    if (IsIconic(root)) ShowWindow(root, SW_RESTORE);
    if (rootOf(GetForegroundWindow()) == root) return 1;

    const DWORD cur = GetCurrentThreadId();
    HWND fg = GetForegroundWindow();
    const DWORD fgTid = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
    if (fgTid && fgTid != cur) {
        AttachThreadInput(cur, fgTid, TRUE);
        SetForegroundWindow(root);
        AttachThreadInput(cur, fgTid, FALSE);
    } else {
        SetForegroundWindow(root);
    }
    if (rootOf(GetForegroundWindow()) == root) return 1;

    // 兜底：一次 Alt 按放能解除系统对 SetForegroundWindow 的限制
    INPUT alt[2] = {};
    alt[0].type = alt[1].type = INPUT_KEYBOARD;
    alt[0].ki.wVk = alt[1].ki.wVk = VK_MENU;
    alt[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, alt, sizeof(INPUT));
    SetForegroundWindow(root);
    return rootOf(GetForegroundWindow()) == root ? 2 : 0;
}

// ---------- 写入 ----------
bool sendUnicode(const std::wstring& text)
{
    std::vector<INPUT> in;
    in.reserve(text.size() * 2);
    for (wchar_t ch : text) {
        INPUT d{};
        d.type = INPUT_KEYBOARD;
        d.ki.wScan = ch;
        d.ki.dwFlags = KEYEVENTF_UNICODE;
        INPUT u = d;
        u.ki.dwFlags |= KEYEVENTF_KEYUP;
        in.push_back(d);
        in.push_back(u);
    }
    const UINT want = (UINT)in.size();
    return SendInput(want, in.data(), sizeof(INPUT)) == want;
}

bool sendMsg(HWND h, UINT msg, WPARAM w, LPARAM l, DWORD_PTR* out = nullptr)
{
    DWORD_PTR r = 0;
    const LRESULT ok = SendMessageTimeoutW(h, msg, w, l, SMTO_ABORTIFHUNG, 500, &r);
    if (out) *out = r;
    return ok != 0;
}

bool isEditClass(const std::wstring& cls)
{
    std::wstring lc = cls;
    for (auto& c : lc) c = (wchar_t)towlower(c);
    return lc == L"edit" || lc.find(L"richedit") != std::wstring::npos;
}

// 现代应用（如 Win11 记事本）的焦点控件是外层容器，真正的 RichEdit 在子孙里
BOOL CALLBACK enumFindEdit(HWND h, LPARAM lp)
{
    auto* out = reinterpret_cast<HWND*>(lp);
    if (IsWindowVisible(h) && isEditClass(classOf(h))) {
        *out = h;
        return FALSE;
    }
    return TRUE;
}

// A 档：不切焦点。把选区移到文末插入，再把用户选区恢复回去。
bool injectEdit(HWND targetRoot, const std::wstring& text, std::string& info)
{
    HWND h = focusChildOf(targetRoot);
    if (!isEditClass(classOf(h))) {
        HWND found = nullptr;
        EnumChildWindows(targetRoot, enumFindEdit, reinterpret_cast<LPARAM>(&found));
        if (found) h = found;
    }
    std::wstring cls = classOf(h);
    info = "控件类名=" + u8(cls);
    std::wstring lc = cls;
    for (auto& c : lc) c = (wchar_t)towlower(c);
    if (!(lc == L"edit" || lc.find(L"richedit") != std::wstring::npos)) {
        info += "（不是 Edit/RichEdit，A 档不适用）";
        return false;
    }
    DWORD selStart = 0, selEnd = 0;
    DWORD_PTR len = 0;
    if (!sendMsg(h, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd) ||
        !sendMsg(h, WM_GETTEXTLENGTH, 0, 0, &len)) {
        info += "（读选区/长度超时）";
        return false;
    }
    sendMsg(h, EM_SETSEL, len, len);
    const bool ok = sendMsg(h, EM_REPLACESEL, TRUE, (LPARAM)text.c_str());   // TRUE = 可撤销
    sendMsg(h, EM_SETSEL, selStart, selEnd);                                   // 插入在文末，不会让用户选区偏移
    char b[96];
    snprintf(b, sizeof(b), " 用户选区=[%lu,%lu] 文长=%llu", selStart, selEnd, (unsigned long long)len);
    info += b;
    return ok;
}

// ---------- P 档：向 Chromium 渲染子窗口直接投递 WM_CHAR，不切焦点 ----------
struct ChildScan {
    std::vector<HWND> renderWidgets;
    std::vector<std::wstring> seen;   // 诊断用：目标下所有子窗口类名
};

BOOL CALLBACK enumChild(HWND h, LPARAM lp)
{
    auto* s = reinterpret_cast<ChildScan*>(lp);
    const std::wstring cls = classOf(h);
    s->seen.push_back(cls);
    if (cls == L"Chrome_RenderWidgetHostHWND") s->renderWidgets.push_back(h);
    return TRUE;
}

// charMsg: "char" = WM_CHAR，"imechar" = WM_IME_CHAR
bool injectPost(HWND targetRoot, const std::wstring& text, const std::wstring& charMsg, bool fakeFocus, std::string& info)
{
    ChildScan scan;
    EnumChildWindows(targetRoot, enumChild, reinterpret_cast<LPARAM>(&scan));
    if (scan.renderWidgets.empty()) {
        info = "没找到 Chrome_RenderWidgetHostHWND；子窗口类名:";
        for (const auto& c : scan.seen) info += " " + u8(c);
        return false;
    }
    HWND h = scan.renderWidgets.front();
    char b[96];
    snprintf(b, sizeof(b), "渲染子窗口=%p(共%zu个) 前台是目标=%s ", (void*)h, scan.renderWidgets.size(),
             rootOf(GetForegroundWindow()) == targetRoot ? "是" : "否");
    info = b;

    const UINT msg = (charMsg == L"imechar") ? WM_IME_CHAR : WM_CHAR;
    bool ok = true;
    // 假焦点：Chrome 靠渲染子窗口收到的 WM_SETFOCUS/WM_KILLFOCUS 判断页面是否有焦点。
    // 同一个窗口的投递消息按顺序处理，所以 SETFOCUS → 字符 → KILLFOCUS 不会乱序。
    if (fakeFocus) ok &= PostMessageW(h, WM_SETFOCUS, 0, 0) != 0;
    for (wchar_t ch : text) {
        ok &= PostMessageW(h, msg, (WPARAM)ch, 1) != 0;
    }
    if (fakeFocus) ok &= PostMessageW(h, WM_KILLFOCUS, 0, 0) != 0;
    info += (msg == WM_CHAR) ? "WM_CHAR" : "WM_IME_CHAR";
    if (fakeFocus) info += "+假焦点";
    return ok;
}

// D 档：切焦点写入再切回。
struct SwitchResult {
    bool injected = false;
    double waitedIdleMs = 0;
    double switchInMs = 0;
    double totalMs = 0;
    int focusIn = 0;
    int focusBack = 0;
    int replayed = 0;
    int swallowed = 0;
    bool wasForeground = false;
    bool skipped = false;
};

SwitchResult injectSwitch(HWND targetRoot, const std::wstring& text, DWORD idleNeedMs, DWORD settleMs, DWORD waitMaxMs,
                          DWORD postBaseMs, DWORD postPerCharMs)
{
    SwitchResult r;

    // 1. 等键盘空闲，且鼠标没在拖拽
    const double w0 = nowMs();
    while (idleMs() < idleNeedMs || (GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
        if (nowMs() - w0 > waitMaxMs) {
            r.skipped = true;
            r.waitedIdleMs = nowMs() - w0;
            return r;
        }
        Sleep(10);
    }
    r.waitedIdleMs = nowMs() - w0;

    HWND prev = rootOf(GetForegroundWindow());
    if (prev == targetRoot) {   // 用户本来就在目标里：直接写
        r.wasForeground = true;
        r.injected = sendUnicode(text);
        return r;
    }

    // 2. 开始吞用户按键 → 切过去 → 写 → 切回 → 重放
    const double t0 = nowMs();
    g_swallowedCount = 0;
    g_swallow = true;

    r.focusIn = forceFocus(targetRoot);
    if (r.focusIn) {
        Sleep(settleMs);   // 给目标窗口恢复自己焦点控件的时间
        r.switchInMs = nowMs() - t0;
        r.injected = sendUnicode(text);
        // SendInput 只是入队，返回时目标可能还没处理完；切回太早，剩下的字会漏进用户正在用的窗口。
        // 所以等待时间随字数增长
        Sleep(postBaseMs + postPerCharMs * (DWORD)text.size());
    }
    if (prev && IsWindow(prev)) r.focusBack = forceFocus(prev);

    replayBuffered(r.replayed);
    r.swallowed = g_swallowedCount.load();
    r.totalMs = nowMs() - t0;
    return r;
}

void countdown(const char* what, int seconds)
{
    for (int i = seconds; i > 0; --i) {
        logf("  %s … %d", what, i);
        Sleep(1000);
    }
}

}   // namespace

int wmain(int argc, wchar_t** argv)
{
    SetConsoleOutputCP(CP_UTF8);

    std::wstring mode = argc > 1 ? argv[1] : L"";
    int count = 5;
    DWORD interval = 4000, idle = 400, settle = 30, waitMax = 15000, postBase = 20, postPerChar = 3;
    std::wstring text = L"语音测试 voice test, 标点。";
    std::wstring charMsg = L"char";
    bool fakeFocus = false;
    for (int i = 2; i + 1 < argc; i += 2) {
        std::wstring k = argv[i];
        if (k == L"--count") count = _wtoi(argv[i + 1]);
        else if (k == L"--interval") interval = (DWORD)_wtoi(argv[i + 1]);
        else if (k == L"--idle") idle = (DWORD)_wtoi(argv[i + 1]);
        else if (k == L"--settle") settle = (DWORD)_wtoi(argv[i + 1]);
        else if (k == L"--wait") waitMax = (DWORD)_wtoi(argv[i + 1]);
        else if (k == L"--post") postBase = (DWORD)_wtoi(argv[i + 1]);
        else if (k == L"--perchar") postPerChar = (DWORD)_wtoi(argv[i + 1]);
        else if (k == L"--text") text = argv[i + 1];
        else if (k == L"--msg") charMsg = argv[i + 1];
        else if (k == L"--fakefocus") fakeFocus = _wtoi(argv[i + 1]) != 0;
    }
    if (mode != L"edit" && mode != L"switch" && mode != L"post") {
        logf("用法: probe.exe <edit|switch|post> [--msg char|imechar] [--count N] [--interval ms] [--idle ms] [--settle ms] [--wait ms] [--text 文本]");
        return 1;
    }

    std::thread(hookThreadMain).detach();

    logf("== 双光标 P0 原型：模式 %s ==", u8(mode).c_str());
    logf("步骤 1：现在切到【目标窗口】（如 Google 文档），并点进正文。");
    countdown("记录目标", 5);
    HWND target = rootOf(GetForegroundWindow());
    if (!target) {
        logf("没有前台窗口");
        return 2;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(target, &pid);
    logf("目标: \"%s\"  类名=%s  pid=%lu", u8(titleOf(target)).c_str(), u8(classOf(target)).c_str(), pid);
    logf("      焦点控件类名=%s", u8(classOf(focusChildOf(target))).c_str());

    logf("步骤 2：现在切到【另一个窗口 B】并持续打字（模拟你同时在别处输入）。");
    countdown("开始注入", 5);

    int okCount = 0;
    for (int n = 1; n <= count; ++n) {
        const std::wstring payload = L"【#" + std::to_wstring(n) + L"】" + text;
        if (mode == L"post") {
            std::string info;
            const double t = nowMs();
            const bool ok = injectPost(target, payload, charMsg, fakeFocus, info);
            logf("#%d post    %s  %.1fms  %s", n, ok ? "已投递" : "FAIL", nowMs() - t, info.c_str());
            if (ok) ++okCount;
        } else if (mode == L"edit") {
            std::string info;
            const double t = nowMs();
            const bool ok = injectEdit(target, payload, info);
            logf("#%d edit    %s  %.1fms  %s", n, ok ? "OK  " : "FAIL", nowMs() - t, info.c_str());
            if (ok) ++okCount;
        } else {
            const SwitchResult r = injectSwitch(target, payload, idle, settle, waitMax, postBase, postPerChar);
            if (r.skipped) {
                logf("#%d switch  跳过：等了 %.0fms 键盘仍没空闲（用户一直在打字）", n, r.waitedIdleMs);
            } else if (r.wasForeground) {
                logf("#%d switch  目标本来就在前台，直接写入 %s", n, r.injected ? "OK" : "FAIL");
                if (r.injected) ++okCount;
            } else {
                logf("#%d switch  %s  等空闲=%.0fms 切入=%.0fms(焦点法%d) 总计=%.0fms 切回=%s 吞键=%d 重放=%d",
                     n, r.injected ? "OK  " : "FAIL", r.waitedIdleMs, r.switchInMs, r.focusIn, r.totalMs,
                     r.focusBack ? "成功" : "失败", r.swallowed, r.replayed);
                if (r.injected) ++okCount;
            }
        }
        if (n < count) Sleep(interval);
    }
    logf("== 结束：成功 %d/%d ==", okCount, count);
    logf("请人工核对：目标里是否按序出现 【#1】…【#%d】、中文/标点是否完整；窗口 B 里你打的字是否一个不少且没串进目标。", count);
    return 0;
}
