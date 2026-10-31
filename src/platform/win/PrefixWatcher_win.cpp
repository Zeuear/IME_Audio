#include "services/PrefixWatcher.h"

#include <QElapsedTimer>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QtCore/qnamespace.h>
#include <atomic>
#include <thread>
#include <vector>
#include <windows.h>

#include "utils/Logger.h"

namespace {

constexpr int kBufferSize = 12;
constexpr qint64 kBufferTimeoutMs = 2500;

// 低级钩子回调是普通函数指针，拿不到 this，只能走全局。同一时刻只会有一个启用中的 PrefixWatcher::Impl
std::atomic<void*> g_active{nullptr};

// 键盘按键 -> 字符。直接按虚拟键映射，不用 ToUnicode：中文输入法开着时钩子仍能看到物理按键，
// 这样 “、、” 也会按 “//” 识别，且不会吃掉输入法或死键的状态
QChar vkToChar(DWORD vk, bool shift) {
    switch (vk) {
    case VK_OEM_2:      return shift ? '?' : '/';
    case VK_OEM_1:      return shift ? ':' : ';';
    case VK_OEM_3:      return shift ? '~' : '`';
    case VK_OEM_5:      return shift ? '|' : '\\';
    case VK_OEM_MINUS:  return shift ? '_' : '-';
    case VK_OEM_PLUS:   return shift ? '+' : '=';
    case VK_OEM_4:      return shift ? '{' : '[';
    case VK_OEM_6:      return shift ? '}' : ']';
    case VK_OEM_7:      return shift ? '"' : '\'';
    case VK_OEM_COMMA:  return shift ? '<' : ',';
    case VK_OEM_PERIOD: return shift ? '>' : '.';
    case VK_SPACE:      return ' ';
    default: break;
    }
    if (vk >= '0' && vk <= '9') {
        static const char kShifted[] = ")!@#$%^&*(";
        return shift ? QChar(kShifted[vk - '0']) : QChar(static_cast<char16_t>(vk));
    }
    if (vk >= 'A' && vk <= 'Z') return shift ? QChar(static_cast<char16_t>(vk)) : QChar(static_cast<char16_t>(vk - 'A' + 'a'));
    return QChar();
}

bool keyDown(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

} // namespace

struct PrefixWatcher::Impl {
    PrefixWatcher* owner = nullptr;

    QMutex mutex;   // 保护前缀表和缓冲：钩子线程读写，界面线程改配置
    QStringList prefixes;
    QString buffer;
    QElapsedTimer sinceLastKey;
    bool queryMode = false;   // 命中前缀后，之后的按键都当搜索词（受 mutex 保护）
    QString query;

    std::thread thread;
    std::atomic<DWORD> threadId{0};
    HHOOK hook = nullptr;

    void start() {
        if (thread.joinable()) return;
        std::atomic<bool> ready{false};
        thread = std::thread([this, &ready]() {
            threadId = GetCurrentThreadId();
            g_active = this;
            hook = SetWindowsHookExW(WH_KEYBOARD_LL, &Impl::hookProc, GetModuleHandleW(nullptr), 0);
            if (!hook) LOG_WARN("PrefixWatcher: SetWindowsHookEx failed");
            ready = true;

            // 低级钩子要求安装它的线程有消息循环
            MSG msg;
            while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            if (hook) UnhookWindowsHookEx(hook);
            hook = nullptr;
            g_active = nullptr;
        });
        while (!ready) std::this_thread::yield();
    }

    void stop() {
        if (!thread.joinable()) return;
        PostThreadMessageW(threadId, WM_QUIT, 0, 0);
        thread.join();
        threadId = 0;
        QMutexLocker lock(&mutex);
        buffer.clear();
    }

    // 返回 true 表示吞掉这次按键（不再交给目标应用）
    bool onKey(DWORD vk, DWORD scanFlags) {
        // 我们自己模拟的按键（退格、Ctrl+V）不参与识别
        if (scanFlags & LLKHF_INJECTED) return false;
        if (isModifier(vk)) return false;   // 单独的修饰键不产生字符，也不打断已输入的内容

        QString hitPrefix;
        QString newQuery;
        bool queryDirty = false;
        int nav = 0;
        bool swallow = false;
        {
            QMutexLocker lock(&mutex);
            const bool chord = keyDown(VK_CONTROL) || keyDown(VK_MENU) || keyDown(VK_LWIN) || keyDown(VK_RWIN);

            if (queryMode) {
                if (vk == VK_UP || vk == VK_DOWN) {
                    nav = vk == VK_UP ? Qt::Key_Up : Qt::Key_Down;
                    swallow = true;
                } else if (vk == VK_RETURN || vk == VK_TAB) {
                    nav = Qt::Key_Return;
                    swallow = true;
                    queryMode = false;
                } else if (vk == VK_ESCAPE) {
                    nav = Qt::Key_Escape;
                    swallow = true;
                    queryMode = false;
                } else if (vk == VK_BACK) {
                    if (query.isEmpty()) {   // 再退一格就删到前缀本身了：放弃
                        nav = Qt::Key_Escape;
                        queryMode = false;
                    } else {
                        query.chop(1);
                        newQuery = query;
                        queryDirty = true;
                    }
                } else {
                    const QChar ch = chord ? QChar() : vkToChar(vk, keyDown(VK_SHIFT));
                    if (ch.isNull()) {   // 方向键左右、Home/End、组合键等会让光标离开，搜索词和目标窗口对不上了
                        nav = Qt::Key_Escape;
                        queryMode = false;
                    } else {
                        query.append(ch);
                        newQuery = query;
                        queryDirty = true;
                    }
                }
            } else {
                if (!sinceLastKey.isValid() || sinceLastKey.elapsed() > kBufferTimeoutMs) buffer.clear();
                sinceLastKey.restart();

                if (vk == VK_BACK) {
                    buffer.chop(1);
                } else if (vk == VK_RETURN || vk == VK_ESCAPE || chord) {
                    buffer.clear();
                } else {
                    const QChar ch = vkToChar(vk, keyDown(VK_SHIFT));
                    if (ch.isNull()) {
                        buffer.clear();
                    } else {
                        buffer.append(ch);
                        if (buffer.size() > kBufferSize) buffer.remove(0, buffer.size() - kBufferSize);
                        for (const QString& p : std::as_const(prefixes)) {
                            if (!p.isEmpty() && buffer.endsWith(p)) {
                                hitPrefix = p;
                                buffer.clear();
                                queryMode = true;
                                query.clear();
                                break;
                            }
                        }
                    }
                }
            }
        }
        // 钩子里不能做耗时的事，交回界面线程处理
        if (!hitPrefix.isEmpty())
            QMetaObject::invokeMethod(owner, [o = owner, hitPrefix]() { emit o->prefixTyped(hitPrefix); }, Qt::QueuedConnection);
        if (queryDirty)
            QMetaObject::invokeMethod(owner, [o = owner, newQuery]() { emit o->queryChanged(newQuery); }, Qt::QueuedConnection);
        if (nav)
            QMetaObject::invokeMethod(owner, [o = owner, nav]() { emit o->navKey(nav); }, Qt::QueuedConnection);
        return swallow;
    }

    static bool isModifier(DWORD vk) {
        return vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_CONTROL || vk == VK_LCONTROL
            || vk == VK_RCONTROL || vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU || vk == VK_LWIN
            || vk == VK_RWIN || vk == VK_CAPITAL;
    }

    // 被吞掉的键，松开时也要吞，否则目标应用只收到孤零零的 KeyUp
    DWORD swallowedVk = 0;

    static LRESULT CALLBACK hookProc(int code, WPARAM wParam, LPARAM lParam) {
        if (code == HC_ACTION) {
            if (auto* self = static_cast<Impl*>(g_active.load())) {
                const auto* kb = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
                if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                    if (self->onKey(kb->vkCode, kb->flags)) {
                        self->swallowedVk = kb->vkCode;
                        return 1;
                    }
                } else if ((wParam == WM_KEYUP || wParam == WM_SYSKEYUP) && self->swallowedVk == kb->vkCode) {
                    self->swallowedVk = 0;
                    return 1;
                }
            }
        }
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }
};

PrefixWatcher::PrefixWatcher(QObject* parent) : QObject(parent), m_impl(std::make_unique<Impl>()) {
    m_impl->owner = this;
}

PrefixWatcher::~PrefixWatcher() {
    m_impl->stop();
}

bool PrefixWatcher::isSupported() { return true; }

void PrefixWatcher::setPrefixes(const QStringList& prefixes) {
    QMutexLocker lock(&m_impl->mutex);
    m_impl->prefixes = prefixes;
}

void PrefixWatcher::setEnabled(bool enabled) {
    if (enabled) m_impl->start();
    else m_impl->stop();
}

void PrefixWatcher::endQuery() {
    QMutexLocker lock(&m_impl->mutex);
    m_impl->queryMode = false;
    m_impl->query.clear();
    m_impl->buffer.clear();
}

void PrefixWatcher::eraseTyped(int count) {
    if (count <= 0) return;
    // 先松开可能还按着的修饰键，否则退格会变成 Ctrl+Backspace（删整个词）之类
    std::vector<INPUT> inputs;
    for (int vk : { VK_SHIFT, VK_CONTROL, VK_MENU }) {
        if (!(GetAsyncKeyState(vk) & 0x8000)) continue;
        INPUT up{};
        up.type = INPUT_KEYBOARD;
        up.ki.wVk = static_cast<WORD>(vk);
        up.ki.dwFlags = KEYEVENTF_KEYUP;
        inputs.push_back(up);
    }
    for (int i = 0; i < count; ++i) {
        INPUT down{};
        down.type = INPUT_KEYBOARD;
        down.ki.wVk = VK_BACK;
        INPUT up = down;
        up.ki.dwFlags = KEYEVENTF_KEYUP;
        inputs.push_back(down);
        inputs.push_back(up);
    }
    SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
}
