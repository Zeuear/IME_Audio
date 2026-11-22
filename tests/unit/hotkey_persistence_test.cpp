#include <gtest/gtest.h>
#include <QApplication>
#include <QSettings>
#include <QTemporaryDir>

#include "config/ConfigManager.h"
#include <QCheckBox>
#include "ui/controls/ToggleLink.h"
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

// 回归：启动时 ToggleLink 把稿子页的复选框同步成设置页复选框的初始状态，会发出 toggled。
// 此时配置还没载入（内存里是默认值），如果处理函数在这时 save()，就会把默认值整份写回 ini，
// 用户的所有设置（快捷键、密钥、模型……）在每次启动时被重置。
TEST(StartupDoesNotOverwriteConfig, ToggleLinkInitialSyncMustNotSaveDefaultsOverUserIni) {
    ASSERT_NE(qobject_cast<QApplication*>(qApp), nullptr);
    QTemporaryDir dir;
    const QString oldPath = ConfigManager::configFilePath();
    const QString path = dir.filePath("user.ini");
    ConfigManager::setConfigFilePath(path);
    {
        QSettings s(path, QSettings::IniFormat);
        s.setValue("hotkey", "Ctrl+Shift+K");
        s.setValue("groqKey", "SECRET");
        s.setValue("injectToForeground", true);
    }

    // 与 MainWin 的接线顺序一致：稿子页复选框默认勾选，设置页复选框在 .ui 里默认不勾选
    QCheckBox transcriptCheck; transcriptCheck.setChecked(true);
    QCheckBox settingsCheck;
    QObject::connect(&transcriptCheck, &QCheckBox::toggled, &transcriptCheck, [](bool on) {
        // 与 MainWin.cpp 里 injectToggled 的处理函数相同：启动阶段（未载入配置）也会走到这里
        ConfigManager::instance().config().injectToForeground = on;
        ConfigManager::instance().save();
    });
    ToggleLink::connect(&transcriptCheck, &settingsCheck);

    QSettings after(path, QSettings::IniFormat);
    EXPECT_EQ(after.value("hotkey").toString(), "Ctrl+Shift+K");
    EXPECT_EQ(after.value("groqKey").toString(), "SECRET");
    EXPECT_TRUE(after.value("injectToForeground").toBool());
    ConfigManager::setConfigFilePath(oldPath);
}

// 兜底：没载入过的实例不能覆盖已存在的配置；载入之后可以正常保存
TEST(StartupDoesNotOverwriteConfig, SaveBeforeLoadIsRefusedButAllowedAfterLoad) {
    QTemporaryDir dir;
    const QString oldPath = ConfigManager::configFilePath();
    const QString path = dir.filePath("guard.ini");
    ConfigManager::setConfigFilePath(path);
    { QSettings s(path, QSettings::IniFormat); s.setValue("groqKey", "SECRET"); }

    ConfigManager m;
    EXPECT_FALSE(m.save());
    EXPECT_EQ(QSettings(path, QSettings::IniFormat).value("groqKey").toString(), "SECRET");

    ASSERT_TRUE(m.load());
    m.config().barMode = true;
    EXPECT_TRUE(m.save());
    QSettings after(path, QSettings::IniFormat);
    EXPECT_EQ(after.value("groqKey").toString(), "SECRET");
    EXPECT_TRUE(after.value("barMode").toBool());
    ConfigManager::setConfigFilePath(oldPath);
}

TEST(StartupDoesNotOverwriteConfig, FirstRunWithoutFileStillWritesDefaults) {
    QTemporaryDir dir;
    const QString oldPath = ConfigManager::configFilePath();
    ConfigManager::setConfigFilePath(dir.filePath("first.ini"));
    ConfigManager m;
    m.applyDefaults();
    EXPECT_TRUE(m.save());
    EXPECT_TRUE(QFile::exists(dir.filePath("first.ini")));
    ConfigManager::setConfigFilePath(oldPath);
}
