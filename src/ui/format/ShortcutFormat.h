#pragma once

#include <QRegularExpression>
#include <QString>
#include <QStringList>

// 快捷键字符串的解析与拼装。刻意与 Qt Widgets 解耦，方便单元测试。
// 存储格式统一使用 Qt 认识的修饰键名（Ctrl/Shift/Alt/Meta），交由 QKeySequence
// 在各平台自动映射——macOS 上 Ctrl 即 ⌘、Meta 即 ⌃。
struct ShortcutParts {
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
    bool meta = false;
    QString key;
};

// 单字符按键统一大写（a -> A）；具名按键统一首字母大写（space -> Space, f5 -> F5），
// 以便配置文件中的历史大小写变体能稳定往返。
inline QString normaliseKeyName(const QString& key) {
    if (key.size() <= 1) return key.toUpper();
    return key.left(1).toUpper() + key.mid(1).toLower();
}

inline ShortcutParts parseShortcut(const QString& shortcut) {
    ShortcutParts parts;
    const QStringList segments = shortcut.split('+', Qt::SkipEmptyParts);
    for (const QString& segment : segments) {
        const QString s = segment.trimmed();
        if (s.isEmpty()) continue;

        if (s.compare("Ctrl", Qt::CaseInsensitive) == 0) parts.ctrl = true;
        else if (s.compare("Shift", Qt::CaseInsensitive) == 0) parts.shift = true;
        else if (s.compare("Alt", Qt::CaseInsensitive) == 0) parts.alt = true;
        // "Win" 是本项目早期版本写入配置文件的别名，需长期兼容
        else if (s.compare("Meta", Qt::CaseInsensitive) == 0
                 || s.compare("Win", Qt::CaseInsensitive) == 0) parts.meta = true;
        else parts.key = normaliseKeyName(s);
    }
    return parts;
}

// 输入框支持粘贴，无法只靠 keyPressEvent 过滤，因此拼装前再校验一次：
// 放进 QKeySequence 的按键名必须是编辑器真正能产生的那几类，否则解析失败会导致
// 热键静默注册不上。
inline bool isSupportedKeyName(const QString& key) {
    static const QRegularExpression pattern(
        QStringLiteral("^([A-Z0-9]|F([1-9]|1[0-9]|2[0-4])|Space)$"));
    return pattern.match(key).hasMatch();
}

inline QString formatShortcut(const ShortcutParts& parts) {
    if (!isSupportedKeyName(parts.key)) return QString();

    QStringList segments;
    if (parts.ctrl) segments << "Ctrl";
    if (parts.shift) segments << "Shift";
    if (parts.alt) segments << "Alt";
    if (parts.meta) segments << "Meta";
    segments << parts.key;
    return segments.join('+');
}
