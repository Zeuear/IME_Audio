#pragma once
#include <QString>
#include <QStringList>
#include "LiveOverlay.h"

// @brief 外观设置（主题、界面语言）的键、枚举与翻译文件的映射，纯逻辑；键沿用旧写法以免旧配置丢失。
namespace Appearance {

// 主题；不认识的值按浅色
LiveOverlay::Theme themeFromKey(const QString& key);
QString themeKey(LiveOverlay::Theme theme);

// 下拉框顺序：浅色、灰色、深色；越界按浅色
QStringList themeKeys();
int themeIndex(LiveOverlay::Theme theme);
LiveOverlay::Theme themeAt(int index);

// 语言，下拉框顺序：跟随系统、中文、English；不认识的键按跟随系统，保证选项与实际翻译一致
QStringList languageKeys();
int languageIndex(const QString& key);

// 该语言键对应的翻译资源；"跟随系统"只有系统区域恰好是 zh_CN 才用中文，其余用英文
QString translationResource(const QString& languageKey, const QString& systemLocaleName);

} // namespace Appearance
