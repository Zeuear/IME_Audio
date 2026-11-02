#include "services/PrefixWatcher.h"

#include <QElapsedTimer>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QtCore/qnamespace.h>
#include <atomic>
#include <thread>
#include <unistd.h>

#import <ApplicationServices/ApplicationServices.h>

#include "utils/Logger.h"
#include "utils/PlatformPermissions.h"

namespace {

constexpr int kBufferSize = 12;
constexpr qint64 kBufferTimeoutMs = 2500;

// macOS 虚拟键码（与键盘布局无关的物理键位，对应 Carbon 的 kVK_*）
enum : int64_t {
    kVkReturn = 0x24, kVkTab = 0x30, kVkSpace = 0x31, kVkBackspace = 0x33, kVkEscape = 0x35,
    kVkKeypadEnter = 0x4C, kVkDown = 0x7D, kVkUp = 0x7E,
};

// 键盘按键 -> 字符。和 Windows 一样直接按物理键映射，不走输入法：中文输入法开着时 “、、” 也按 “//” 识别
QChar keyToChar(int64_t key, bool shift) {
    switch (key) {
    case 0x2C: return shift ? '?' : '/';
    case 0x29: return shift ? ':' : ';';
    case 0x32: return shift ? '~' : '`';
    case 0x2A: return shift ? '|' : '\\';
    case 0x1B: return shift ? '_' : '-';
    case 0x18: return shift ? '+' : '=';
    case 0x21: return shift ? '{' : '[';
    case 0x1E: return shift ? '}' : ']';
    case 0x27: return shift ? '"' : '\'';
    case 0x2B: return shift ? '<' : ',';
    case 0x2F: return shift ? '>' : '.';
    case kVkSpace: return ' ';
    default: break;
    }

    // 数字行：键码不连续，按 0..9 排列
    static const int64_t kDigitKeys[10] = { 0x1D, 0x12, 0x13, 0x14, 0x15, 0x17, 0x16, 0x1A, 0x1C, 0x19 };
    static const char kShiftedDigits[] = ")!@#$%^&*(";
    for (int d = 0; d < 10; ++d) {
        if (key == kDigitKeys[d]) return shift ? QChar(kShiftedDigits[d]) : QChar('0' + d);
    }

    // 字母：同样不连续，用 "键码 -> 字母" 表
    static const struct { int64_t key; char letter; } kLetters[] = {
        {0x00,'a'},{0x0B,'b'},{0x08,'c'},{0x02,'d'},{0x0E,'e'},{0x03,'f'},{0x05,'g'},{0x04,'h'},{0x22,'i'},
        {0x26,'j'},{0x28,'k'},{0x25,'l'},{0x2E,'m'},{0x2D,'n'},{0x1F,'o'},{0x23,'p'},{0x0C,'q'},{0x0F,'r'},
        {0x01,'s'},{0x11,'t'},{0x20,'u'},{0x09,'v'},{0x0D,'w'},{0x07,'x'},{0x10,'y'},{0x06,'z'},
    };
    for (const auto& l : kLetters) {
        if (key == l.key) return QChar(shift ? l.letter - 'a' + 'A' : l.letter);
    }
    return QChar();
}

} // namespace

struct PrefixWatcher::Impl {
    PrefixWatcher* owner = nullptr;

    QMutex mutex;   // 保护前缀表和缓冲：事件线程读写，界面线程改配置
    QStringList prefixes;
    QString buffer;
    QElapsedTimer sinceLastKey;
    bool queryMode = false;   // 命中前缀后，之后的按键都当搜索词（受 mutex 保护）
    QString query;

    std::thread thread;
    CFRunLoopRef runLoop = nullptr;
    CFMachPortRef tap = nullptr;
    int64_t swallowedKey = -1;   // 被吞掉的键，松开时也要吞，否则目标应用只收到孤零零的 KeyUp

    void start() {
        if (thread.joinable()) return;
        // 没授权就先弹系统的辅助功能引导框；授权要重启应用才生效，这次的 tap 仍会创建失败（只记日志）
        if (!PlatformPermissions::requestAccessibility(true)) LOG_WARN("PrefixWatcher(mac): accessibility not granted yet");
        // 全局监听键盘按键还要“输入监控”权限：没有它 tap 能创建成功，但收不到任何按键，前缀监听就悄悄失效。
        // 没授权时触发系统授权框，并在系统设置的“输入监控”里生成条目
        if (!CGPreflightListenEventAccess()) {
            LOG_WARN("PrefixWatcher(mac): input monitoring not granted, requesting");
            CGRequestListenEventAccess();
        }
        std::atomic<bool> ready{false};
        thread = std::thread([this, &ready]() {
            // 主动拦截（能吞键）需要辅助功能权限；没授权时 CGEventTapCreate 返回空，只记日志，不影响其它功能
            tap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
                                   CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp),
                                   &Impl::tapCallback, this);
            if (!tap) {
                LOG_WARN("PrefixWatcher(mac): CGEventTapCreate failed, accessibility permission missing?");
                ready = true;
                return;
            }
            CFRunLoopSourceRef source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0);
            runLoop = CFRunLoopGetCurrent();
            CFRunLoopAddSource(runLoop, source, kCFRunLoopCommonModes);
            CGEventTapEnable(tap, true);
            LOG_INFO("PrefixWatcher(mac): event tap started");
            ready = true;

            CFRunLoopRun();

            CGEventTapEnable(tap, false);
            CFRunLoopRemoveSource(runLoop, source, kCFRunLoopCommonModes);
            CFRelease(source);
            CFRelease(tap);
            tap = nullptr;
            runLoop = nullptr;
        });
        while (!ready) std::this_thread::yield();
    }

    void stop() {
        if (!thread.joinable()) return;
        if (runLoop) CFRunLoopStop(runLoop);
        thread.join();
        QMutexLocker lock(&mutex);
        buffer.clear();
        queryMode = false;
        query.clear();
    }

    // 返回 true 表示吞掉这次按键（不再交给目标应用）
    bool onKey(int64_t key, CGEventFlags flags) {
        QString hitPrefix;
        QString newQuery;
        bool queryDirty = false;
        int nav = 0;
        bool swallow = false;
        {
            QMutexLocker lock(&mutex);
            const bool chord = flags & (kCGEventFlagMaskControl | kCGEventFlagMaskAlternate | kCGEventFlagMaskCommand);
            const bool shift = flags & kCGEventFlagMaskShift;

            if (queryMode) {
                if (key == kVkUp || key == kVkDown) {
                    nav = key == kVkUp ? Qt::Key_Up : Qt::Key_Down;
                    swallow = true;
                } else if (key == kVkReturn || key == kVkKeypadEnter || key == kVkTab) {
                    nav = Qt::Key_Return;
                    swallow = true;
                    queryMode = false;
                } else if (key == kVkEscape) {
                    nav = Qt::Key_Escape;
                    swallow = true;
                    queryMode = false;
                } else if (key == kVkBackspace) {
                    if (query.isEmpty()) {   // 再退一格就删到前缀本身了：放弃
                        nav = Qt::Key_Escape;
                        queryMode = false;
                    } else {
                        query.chop(1);
                        newQuery = query;
                        queryDirty = true;
                    }
                } else {
                    const QChar ch = chord ? QChar() : keyToChar(key, shift);
                    if (ch.isNull()) {   // 左右方向键、组合键等会让光标离开，搜索词和目标窗口对不上了
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

                if (key == kVkBackspace) {
                    buffer.chop(1);
                } else if (key == kVkReturn || key == kVkKeypadEnter || key == kVkEscape || chord) {
                    buffer.clear();
                } else {
                    const QChar ch = keyToChar(key, shift);
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
        // 事件回调里不能做耗时的事，交回界面线程处理
        if (!hitPrefix.isEmpty()) LOG_DEBUG(QString("PrefixWatcher(mac): prefix typed: %1").arg(hitPrefix));
        if (!hitPrefix.isEmpty())
            QMetaObject::invokeMethod(owner, [o = owner, hitPrefix]() { emit o->prefixTyped(hitPrefix); }, Qt::QueuedConnection);
        if (queryDirty)
            QMetaObject::invokeMethod(owner, [o = owner, newQuery]() { emit o->queryChanged(newQuery); }, Qt::QueuedConnection);
        if (nav)
            QMetaObject::invokeMethod(owner, [o = owner, nav]() { emit o->navKey(nav); }, Qt::QueuedConnection);
        return swallow;
    }

    static CGEventRef tapCallback(CGEventTapProxy, CGEventType type, CGEventRef event, void* refcon) {
        auto* self = static_cast<Impl*>(refcon);
        // 回调超时或用户输入过多时系统会停掉 tap，要重新打开，否则前缀监听就悄悄失效了
        if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
            if (self->tap) CGEventTapEnable(self->tap, true);
            return event;
        }
        if (type != kCGEventKeyDown && type != kCGEventKeyUp) return event;

        // 我们自己模拟的按键（退格、⌘V）不参与识别
        if (CGEventGetIntegerValueField(event, kCGEventSourceUnixProcessID) == getpid()) return event;

        const int64_t key = CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
        if (type == kCGEventKeyUp) {
            if (self->swallowedKey == key) {
                self->swallowedKey = -1;
                return nullptr;
            }
            return event;
        }
        // 长按产生的自动重复也按一次按键处理，保持和 Windows 钩子一致
        if (self->onKey(key, CGEventGetFlags(event))) {
            self->swallowedKey = key;
            return nullptr;
        }
        return event;
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
    for (int i = 0; i < count; ++i) {
        for (bool down : { true, false }) {
            CGEventRef ev = CGEventCreateKeyboardEvent(nullptr, static_cast<CGKeyCode>(kVkBackspace), down);
            if (!ev) continue;
            CGEventSetFlags(ev, 0);   // 清掉仍按着的修饰键，否则退格会变成 ⌥⌫（删整个词）之类
            CGEventPost(kCGHIDEventTap, ev);
            CFRelease(ev);
        }
    }
}
