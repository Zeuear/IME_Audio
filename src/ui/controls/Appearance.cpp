#include "Appearance.h"

namespace Appearance {

namespace {
const QString kChinese = QStringLiteral(":/i18n/zh_CN.qm");
const QString kEnglish = QStringLiteral(":/i18n/qt_EN.qm");
}

LiveOverlay::Theme themeFromKey(const QString& key)
{
    if (key == QLatin1String("dark")) return LiveOverlay::Theme::Dark;
    if (key == QLatin1String("gray")) return LiveOverlay::Theme::Gray;
    return LiveOverlay::Theme::Light;
}

QString themeKey(LiveOverlay::Theme theme)
{
    switch (theme) {
    case LiveOverlay::Theme::Dark: return QStringLiteral("dark");
    case LiveOverlay::Theme::Gray: return QStringLiteral("gray");
    case LiveOverlay::Theme::Light: break;
    }
    return QStringLiteral("light");
}

QStringList themeKeys()
{
    return { QStringLiteral("light"), QStringLiteral("gray"), QStringLiteral("dark") };
}

int themeIndex(LiveOverlay::Theme theme)
{
    return qMax(0, themeKeys().indexOf(themeKey(theme)));
}

LiveOverlay::Theme themeAt(int index)
{
    const QStringList keys = themeKeys();
    return index >= 0 && index < keys.size() ? themeFromKey(keys.at(index)) : LiveOverlay::Theme::Light;
}

QStringList languageKeys()
{
    return { QStringLiteral("System"), QStringLiteral("Chinese"), QStringLiteral("English") };
}

int languageIndex(const QString& key)
{
    return qMax(0, languageKeys().indexOf(key));
}

QString translationResource(const QString& languageKey, const QString& systemLocaleName)
{
    if (languageKey == QLatin1String("Chinese")) return kChinese;
    if (languageKey == QLatin1String("English")) return kEnglish;
    return systemLocaleName == QLatin1String("zh_CN") ? kChinese : kEnglish;
}

} // namespace Appearance
