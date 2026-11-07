#pragma once
#include <QString>
#include <QStringList>
#include "LiveOverlay.h"

// @brief 外观设置（主题、界面语言）的键与枚举、翻译文件之间的映射，只有纯逻辑，便于单元测试。
//        配置文件里存的键沿用原来的写法：主题 "dark" / "gray" / "light"，语言 "System" / "Chinese" / "English"，
//        旧用户的设置不会丢。以前这些判断在读配置和三个菜单槽里重复了四份。
namespace Appearance {

// 主题。不认识的值按浅色，和原来读配置时的 else 分支一致
LiveOverlay::Theme themeFromKey(const QString& key);
QString themeKey(LiveOverlay::Theme theme);

// 下拉框的顺序：浅色、灰色、深色；序号和主题互转，越界按浅色
QStringList themeKeys();
int themeIndex(LiveOverlay::Theme theme);
LiveOverlay::Theme themeAt(int index);

// 语言。下拉框的顺序：跟随系统、中文、English；不认识的键按"跟随系统"，
// 这样配置文件被改坏时，界面显示的选项和实际用的翻译一致
QStringList languageKeys();
int languageIndex(const QString& key);

// 该语言键对应的翻译资源；"跟随系统"只有系统区域恰好是 zh_CN 才用中文，其余用英文
QString translationResource(const QString& languageKey, const QString& systemLocaleName);

} // namespace Appearance
