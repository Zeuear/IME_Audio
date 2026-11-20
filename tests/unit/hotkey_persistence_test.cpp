#include <gtest/gtest.h>
#include <QSettings>
#include <QTemporaryDir>

#include "config/ConfigManager.h"
#include "ui/format/ShortcutFormat.h"

// 快捷键「重新打开就被清空」的复现。
TEST(HotkeyPersistence, ConfigWithoutHotkeyKeyFallsBackToDefault) {
    QTemporaryDir dir;
    const QString oldPath = ConfigManager::configFilePath();
    ConfigManager::setConfigFilePath(dir.filePath("fresh.ini"));
    {
        ConfigManager m;
        m.load();   // 文件不存在：读不到 hotkey 键
        EXPECT_EQ(m.config().hotkey, kDefaultHotkey);
    }
    ConfigManager::setConfigFilePath(oldPath);
}

TEST(HotkeyPersistence, ModifiersWithoutKeyFormatToEmpty) {
    ShortcutParts p;
    p.ctrl = true; p.alt = true;   // 只勾了修饰键，按键框是空的
    // 这个空串会经 shortcutChanged 写进配置，重启后修饰键也丢了
    EXPECT_TRUE(formatShortcut(p).isEmpty());
}

TEST(HotkeyPersistence, EmptyStoredHotkeyFallsBackToDefault) {
    QTemporaryDir dir;
    const QString oldPath = ConfigManager::configFilePath();
    const QString path = dir.filePath("empty.ini");
    ConfigManager::setConfigFilePath(path);
    {
        QSettings s(path, QSettings::IniFormat);
        s.setValue("hotkey", "");   // 旧版本已经把空值写进了配置
    }
    ConfigManager m;
    m.load();
    EXPECT_EQ(m.config().hotkey, kDefaultHotkey);
    ConfigManager::setConfigFilePath(oldPath);
}

TEST(HotkeyPersistence, CustomHotkeySurvivesRestart) {
    QTemporaryDir dir;
    const QString oldPath = ConfigManager::configFilePath();
    ConfigManager::setConfigFilePath(dir.filePath("custom.ini"));
    {
        ConfigManager w;
        w.config().hotkey = "Ctrl+Shift+K";
        ASSERT_TRUE(w.save());
    }
    ConfigManager r;
    r.load();
    EXPECT_EQ(r.config().hotkey, "Ctrl+Shift+K");
    ConfigManager::setConfigFilePath(oldPath);
}
