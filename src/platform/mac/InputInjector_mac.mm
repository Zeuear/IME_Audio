#include "../../InputInjector.h"
#include "../../utils/Logger.h"
#include "../../utils/PlatformPermissions.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <vector>

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>

namespace {

constexpr CGKeyCode kVkAnsiV = 0x09;
constexpr CGKeyCode kVkCommand = 0x37;

constexpr int kUnicodeChunk = 20;
constexpr unsigned long kChunkDelayMs = 2;

// ⌘V 发出后目标应用是异步读取剪贴板的，太早恢复会让它粘到旧内容。
constexpr int kClipboardRestoreMs = 600;

// 粘贴会覆盖用户的剪贴板，所以先存一份快照，粘完再还原。
// 连续注入（连续模式）时只保留最早那份，否则第二次会把"上一句转录"当成用户的剪贴板存下来。
struct ClipboardRestore {
    QMimeData* saved = nullptr;
    QString injectedText;
    QTimer* timer = nullptr;
};

ClipboardRestore& restoreState() {
    static ClipboardRestore s;
    return s;
}

QMimeData* snapshotClipboard(const QClipboard* clipboard) {
    auto* copy = new QMimeData;
    if (const QMimeData* src = clipboard->mimeData()) {
        for (const QString& format : src->formats()) {
            copy->setData(format, src->data(format));
        }
    }
    return copy;
}

void restoreClipboardNow() {
    ClipboardRestore& s = restoreState();
    if (!s.saved) return;

    QClipboard* clipboard = QGuiApplication::clipboard();
    // 用户在这段时间里自己复制了别的东西，就不能再用旧快照覆盖回去。
    if (clipboard && clipboard->text() == s.injectedText) {
        clipboard->setMimeData(s.saved);   // 所有权移交给剪贴板
        LOG_DEBUG("InputInjector(mac): clipboard restored");
    } else {
        delete s.saved;
        LOG_DEBUG("InputInjector(mac): clipboard changed meanwhile, skip restore");
    }
    s.saved = nullptr;
}

void scheduleClipboardRestore(const QString& injectedText) {
    ClipboardRestore& s = restoreState();
    s.injectedText = injectedText;
    if (!s.timer) {
        s.timer = new QTimer;   // 只会在主线程调用
        s.timer->setSingleShot(true);
        QObject::connect(s.timer, &QTimer::timeout, restoreClipboardNow);
    }
    s.timer->start(kClipboardRestoreMs);
}

class EventSource {
public:
    EventSource() : m_ref(CGEventSourceCreate(kCGEventSourceStateHIDSystemState)) {}
    ~EventSource() { if (m_ref) CFRelease(m_ref); }

    EventSource(const EventSource&) = delete;
    EventSource& operator=(const EventSource&) = delete;

    CGEventSourceRef get() const { return m_ref; }
    explicit operator bool() const { return m_ref != nullptr; }

private:
    CGEventSourceRef m_ref;
};

bool ensureAccessibilityTrusted() {
    if (PlatformPermissions::accessibilityStatus() == PlatformPermissions::Status::Granted) {
        return true;
    }

    static const bool s_prompted = PlatformPermissions::requestAccessibility(true);
    (void)s_prompted;

    LOG_ERROR("InputInjector(mac): 缺少「辅助功能」授权，合成的键盘事件会被系统丢弃；"
              "请在系统设置 → 隐私与安全性 → 辅助功能中勾选本应用并重启");
    return false;
}

bool postUnicodeChunk(CGEventSourceRef source, const UniChar* buffer, int length) {
    CGEventRef down = CGEventCreateKeyboardEvent(source, 0, true);
    CGEventRef up = CGEventCreateKeyboardEvent(source, 0, false);
    if (!down || !up) {
        if (down) CFRelease(down);
        if (up) CFRelease(up);
        LOG_ERROR("InputInjector(mac): CGEventCreateKeyboardEvent failed");
        return false;
    }

    // 触发注入的全局热键（默认 ⌃⌥Y）此刻很可能还按着，不把修饰键清零的话，
    // 目标应用会把注入的每个字符解读成快捷键而不是文本。
    CGEventSetFlags(down, static_cast<CGEventFlags>(0));
    CGEventSetFlags(up, static_cast<CGEventFlags>(0));

    CGEventKeyboardSetUnicodeString(down, length, buffer);
    CGEventKeyboardSetUnicodeString(up, length, buffer);

    CGEventPost(kCGHIDEventTap, down);
    CGEventPost(kCGHIDEventTap, up);

    CFRelease(down);
    CFRelease(up);
    return true;
}

// 分批边界不能落在代理对(surrogate pair)中间，否则这个字符会被拆成两个乱码。
int chunkEndFor(const std::vector<UniChar>& units, int begin) {
    int end = std::min(static_cast<int>(units.size()), begin + kUnicodeChunk);
    if (end < static_cast<int>(units.size()) && units[end - 1] >= 0xD800 && units[end - 1] <= 0xDBFF) {
        --end;
    }
    return end > begin ? end : begin + 1;
}

} // namespace

bool InputInjector::sendCtrlV() {
    if (!ensureAccessibilityTrusted()) return false;

    EventSource source;
    if (!source) {
        LOG_ERROR("InputInjector(mac): CGEventSourceCreate failed");
        return false;
    }

    // 只在 v 事件上打 command flag 对多数应用够用，但部分应用（尤其 Electron / 游戏）
    // 会自行跟踪修饰键的按下与抬起，所以完整地合成 ⌘ 的 down/up 更稳。
    struct Step { CGKeyCode key; bool down; CGEventFlags flags; };
    static const Step kSteps[] = {
        { kVkCommand, true,  kCGEventFlagMaskCommand },
        { kVkAnsiV,   true,  kCGEventFlagMaskCommand },
        { kVkAnsiV,   false, kCGEventFlagMaskCommand },
        { kVkCommand, false, static_cast<CGEventFlags>(0) },
    };

    for (const Step& step : kSteps) {
        CGEventRef event = CGEventCreateKeyboardEvent(source.get(), step.key, step.down);
        if (!event) {
            LOG_ERROR("InputInjector(mac): failed to create Command-V event");
            return false;
        }
        CGEventSetFlags(event, step.flags);
        CGEventPost(kCGHIDEventTap, event);
        CFRelease(event);
    }
    return true;
}

bool InputInjector::pasteViaClipboard(const QString& text) {
    if (text.isEmpty()) return false;

    QClipboard* clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        LOG_ERROR("InputInjector(mac): no clipboard available");
        return false;
    }

    LOG_DEBUG(QString("InputInjector(mac): paste via clipboard, %1 chars").arg(text.size()));
    ClipboardRestore& state = restoreState();
    if (!state.saved) {
        state.saved = snapshotClipboard(clipboard);
    }

    clipboard->setText(text);
    // 给 pasteboard 的跨进程同步留出时间：⌘V 到达得比剪贴板内容更新更早的话，
    // 目标应用粘贴的会是上一次的内容。
    QThread::msleep(50);

    if (sendCtrlV()) {
        scheduleClipboardRestore(text);
        return true;
    }

    // ⌘V 没发出去（通常是缺辅助功能授权）：把转录文本留在剪贴板里，用户可以手动 ⌘V。
    delete state.saved;
    state.saved = nullptr;
    LOG_WARN("InputInjector(mac): ⌘V not sent; transcription left in clipboard for manual paste");
    return false;
}

bool InputInjector::pasteViaUnicodeTyping(const QString& text) {
    if (text.isEmpty()) return false;
    if (!ensureAccessibilityTrusted()) return false;

    EventSource source;
    if (!source) {
        LOG_ERROR("InputInjector(mac): CGEventSourceCreate failed");
        return false;
    }

    LOG_DEBUG(QString("InputInjector(mac): unicode typing, %1 UTF-16 units").arg(text.size()));
    const UniChar* utf16 = reinterpret_cast<const UniChar*>(text.utf16());
    std::vector<UniChar> units(utf16, utf16 + text.size());

    int begin = 0;
    while (begin < static_cast<int>(units.size())) {
        const int end = chunkEndFor(units, begin);
        if (!postUnicodeChunk(source.get(), units.data() + begin, end - begin)) {
            LOG_WARN(QString("InputInjector(mac): unicode typing aborted at [%1,%2) of %3")
                         .arg(begin).arg(end).arg(units.size()));
            return false;
        }
        begin = end;
        if (begin < static_cast<int>(units.size())) {
            QThread::msleep(kChunkDelayMs);
        }
    }
    return true;
}

InputInjector::Mode InputInjector::defaultMode() {
    // 拼音等输入法会拦截合成的按键事件：逐字输入的 Unicode 事件带着 keycode 0（字母 a），
    // 输入法按字母键处理，文本被吞掉或变成拼音串。⌘V 不经过输入法，所以默认走剪贴板粘贴。
    return Mode::ClipboardOnly;
}

bool InputInjector::inject(const QString& text, Mode mode) {
    if (text.isEmpty()) return false;

    bool ok = false;
    switch (mode) {
    case Mode::ClipboardOnly:
        ok = pasteViaClipboard(text);
        break;
    case Mode::PreferClipboard:
        ok = pasteViaClipboard(text);
        if (!ok) {
            LOG_WARN("InputInjector(mac): clipboard paste failed, falling back to unicode typing");
            ok = pasteViaUnicodeTyping(text);
        }
        break;
    case Mode::UnicodeTypeOnly:
    default:
        ok = pasteViaUnicodeTyping(text);
        break;
    }

    // CGEventPost 没有返回值：ok 只代表事件已投递，不代表目标应用收到。
    // 目标应用一栏用于定位"焦点在哪个应用"这类问题。
    NSRunningApplication* front = [[NSWorkspace sharedWorkspace] frontmostApplication];
    LOG_DEBUG(QString("InputInjector(mac): inject mode=%1 ok=%2 chars=%3 frontmost=%4")
                  .arg(static_cast<int>(mode)).arg(ok).arg(text.size())
                  .arg(front ? QString::fromNSString(front.bundleIdentifier ?: front.localizedName) : QString("?")));
    return ok;
}
