#include <gtest/gtest.h>

#include "widgets/ShortcutFormat.h"

TEST(ShortcutFormat, ParsesMultiCharacterKeyName) {
    // 回归：旧实现只接受长度为 1 的按键段，"Space" 这类具名按键被静默丢弃，
    // 组合键打开设置页后会退化成只剩修饰键。
    const ShortcutParts parts = parseShortcut("Ctrl+Alt+Space");
    EXPECT_TRUE(parts.ctrl);
    EXPECT_TRUE(parts.alt);
    EXPECT_FALSE(parts.shift);
    EXPECT_FALSE(parts.meta);
    EXPECT_EQ(parts.key, QString("Space"));
}

TEST(ShortcutFormat, ParsesFunctionKey) {
    const ShortcutParts parts = parseShortcut("Ctrl+Shift+F5");
    EXPECT_TRUE(parts.ctrl);
    EXPECT_TRUE(parts.shift);
    EXPECT_EQ(parts.key, QString("F5"));
}

TEST(ShortcutFormat, ParsesSingleLetterAndNormalisesCase) {
    const ShortcutParts parts = parseShortcut("ctrl+a");
    EXPECT_TRUE(parts.ctrl);
    EXPECT_EQ(parts.key, QString("A"));
}

TEST(ShortcutFormat, AcceptsLegacyWinAliasForMeta) {
    // 已保存的配置里存的是 "Win"，必须继续认得，否则升级后热键丢失。
    const ShortcutParts parts = parseShortcut("Win+Alt+K");
    EXPECT_TRUE(parts.meta);
    EXPECT_TRUE(parts.alt);
    EXPECT_EQ(parts.key, QString("K"));
}

TEST(ShortcutFormat, EmitsQtRecognisedMetaName) {
    // 回归：旧实现输出 "Win"，QKeySequence 不认识，热键注册静默失败。
    ShortcutParts parts;
    parts.meta = true;
    parts.key = "K";
    EXPECT_EQ(formatShortcut(parts), QString("Meta+K"));
}

TEST(ShortcutFormat, RoundTripsMacDefault) {
    // macOS 默认热键 ⌃⌥Space：Qt 语义下 Meta 即 ⌃ Control、Alt 即 ⌥ Option。
    const QString mac = "Alt+Meta+Space";
    EXPECT_EQ(formatShortcut(parseShortcut(mac)), mac);
}

TEST(ShortcutFormat, RoundTripsWindowsDefault) {
    const QString win = "Ctrl+Alt+Space";
    EXPECT_EQ(formatShortcut(parseShortcut(win)), win);
}

TEST(ShortcutFormat, ModifierOrderIsCanonical) {
    EXPECT_EQ(formatShortcut(parseShortcut("Alt+Shift+Ctrl+Meta+A")),
              QString("Ctrl+Shift+Alt+Meta+A"));
}

TEST(ShortcutFormat, EmptyInputYieldsEmptyOutput) {
    const ShortcutParts parts = parseShortcut("");
    EXPECT_FALSE(parts.ctrl);
    EXPECT_TRUE(parts.key.isEmpty());
    EXPECT_TRUE(formatShortcut(parts).isEmpty());
}

TEST(ShortcutFormat, RejectsKeyNameQtCannotParse) {
    // 输入框允许粘贴，绕过了按键过滤。若把任意文本拼进快捷键，QKeySequence 解析失败，
    // 热键会静默注册不上。
    ShortcutParts parts;
    parts.ctrl = true;
    parts.key = "Hello";
    EXPECT_TRUE(formatShortcut(parts).isEmpty());
}

TEST(ShortcutFormat, AcceptsEveryKeyTheEditorCanProduce) {
    for (const QString& key : { "A", "Z", "0", "9", "F1", "F24", "Space" }) {
        ShortcutParts parts;
        parts.ctrl = true;
        parts.key = key;
        EXPECT_EQ(formatShortcut(parts), QString("Ctrl+") + key) << "key=" << key.toStdString();
    }
}

TEST(ShortcutFormat, ModifiersOnlyProduceNoSequence) {
    // 没有主按键的组合不是有效热键，不应产出 "Ctrl+Alt" 这种半成品。
    ShortcutParts parts;
    parts.ctrl = true;
    parts.alt = true;
    EXPECT_TRUE(formatShortcut(parts).isEmpty());
}
