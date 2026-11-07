#include <gtest/gtest.h>
#include <QString>
#include <QStringList>

#include "ui/controls/Appearance.h"

using Theme = LiveOverlay::Theme;

// ---- 主题：配置文件里存的是 "dark" / "gray" / "light" ----

TEST(Appearance, ThemeKeysMapToThemes) {
    EXPECT_EQ(Appearance::themeFromKey("dark"), Theme::Dark);
    EXPECT_EQ(Appearance::themeFromKey("gray"), Theme::Gray);
    EXPECT_EQ(Appearance::themeFromKey("light"), Theme::Light);
}

TEST(Appearance, UnknownThemeKeyFallsBackToLightAsBefore) {
    // 和原来 readIni 的 else 分支一致：不认识的值按浅色
    EXPECT_EQ(Appearance::themeFromKey("purple"), Theme::Light);
    EXPECT_EQ(Appearance::themeFromKey(""), Theme::Light);
}

TEST(Appearance, ThemeKeyIsTheInverseOfThemeFromKey) {
    EXPECT_EQ(Appearance::themeKey(Theme::Dark), "dark");
    EXPECT_EQ(Appearance::themeKey(Theme::Gray), "gray");
    EXPECT_EQ(Appearance::themeKey(Theme::Light), "light");
}

TEST(Appearance, ThemeComboOrderIsLightGrayDark) {
    // 下拉框的顺序是界面上的约定，序号和主题一一对应
    EXPECT_EQ(Appearance::themeKeys(), (QStringList{ "light", "gray", "dark" }));
    EXPECT_EQ(Appearance::themeIndex(Theme::Light), 0);
    EXPECT_EQ(Appearance::themeIndex(Theme::Gray), 1);
    EXPECT_EQ(Appearance::themeIndex(Theme::Dark), 2);
    EXPECT_EQ(Appearance::themeAt(0), Theme::Light);
    EXPECT_EQ(Appearance::themeAt(1), Theme::Gray);
    EXPECT_EQ(Appearance::themeAt(2), Theme::Dark);
}

TEST(Appearance, ThemeAtOutOfRangeIsLight) {
    EXPECT_EQ(Appearance::themeAt(-1), Theme::Light);
    EXPECT_EQ(Appearance::themeAt(99), Theme::Light);
}

// ---- 语言：配置文件里存的是 "System" / "Chinese" / "English" ----

TEST(Appearance, LanguageComboOrderIsSystemChineseEnglish) {
    EXPECT_EQ(Appearance::languageKeys(), (QStringList{ "System", "Chinese", "English" }));
    EXPECT_EQ(Appearance::languageIndex("System"), 0);
    EXPECT_EQ(Appearance::languageIndex("Chinese"), 1);
    EXPECT_EQ(Appearance::languageIndex("English"), 2);
}

TEST(Appearance, UnknownLanguageKeyMeansFollowSystem) {
    // 配置文件被改坏时，界面显示"跟随系统"，实际用的翻译也是跟随系统，两者一致
    EXPECT_EQ(Appearance::languageIndex("Klingon"), 0);
    EXPECT_EQ(Appearance::translationResource("Klingon", "zh_CN"), Appearance::translationResource("System", "zh_CN"));
    EXPECT_EQ(Appearance::translationResource("Klingon", "en_US"), Appearance::translationResource("System", "en_US"));
}

TEST(Appearance, ChineseAndEnglishIgnoreTheSystemLocale) {
    EXPECT_EQ(Appearance::translationResource("Chinese", "en_US"), ":/i18n/zh_CN.qm");
    EXPECT_EQ(Appearance::translationResource("Chinese", "zh_CN"), ":/i18n/zh_CN.qm");
    EXPECT_EQ(Appearance::translationResource("English", "zh_CN"), ":/i18n/qt_EN.qm");
    EXPECT_EQ(Appearance::translationResource("English", "en_US"), ":/i18n/qt_EN.qm");
}

TEST(Appearance, FollowSystemUsesChineseOnlyForZhCN) {
    EXPECT_EQ(Appearance::translationResource("System", "zh_CN"), ":/i18n/zh_CN.qm");
    EXPECT_EQ(Appearance::translationResource("System", "en_US"), ":/i18n/qt_EN.qm");
    EXPECT_EQ(Appearance::translationResource("System", "ja_JP"), ":/i18n/qt_EN.qm");
}
