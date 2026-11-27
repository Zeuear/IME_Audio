#include "LanguageRouting.h"

#include <QHash>
#include <QtGlobal>

namespace {
const QHash<QString, QString>& kDisplayNameToCode() {
    static const QHash<QString, QString> map{
        { QStringLiteral("英文"), QStringLiteral("en") },
        { QStringLiteral("日文"), QStringLiteral("ja") },
        { QStringLiteral("韩文"), QStringLiteral("ko") },
        { QStringLiteral("繁体中文"), QStringLiteral("zh-Hant") },
        { QStringLiteral("简体中文"), QStringLiteral("zh-Hans") },
    };
    return map;
}

const QHash<QString, QString>& kCodeToDisplayName() {
    static const QHash<QString, QString> map{
        { QStringLiteral("en"), QStringLiteral("英文") },
        { QStringLiteral("ja"), QStringLiteral("日文") },
        { QStringLiteral("ko"), QStringLiteral("韩文") },
        { QStringLiteral("zh-Hant"), QStringLiteral("繁体中文") },
        { QStringLiteral("zh-Hans"), QStringLiteral("简体中文") },
    };
    return map;
}

bool directionExists(const QSet<QString>& knownPairs, const QString& from, const QString& to) {
    return knownPairs.contains(from + QStringLiteral(">") + to);
}
} // namespace

TranslationRoute planRoute(const QString& from, const QString& to,
                           const QSet<QString>& knownPairs) {
    TranslationRoute route;

    if (from.isEmpty() || to.isEmpty()) {
        route.kind = RouteKind::Unsupported;
        return route;
    }

    if (from == to) {
        route.kind = RouteKind::Same;
        return route;
    }

    if (directionExists(knownPairs, from, to)) {
        route.kind = RouteKind::Direct;
        route.hops.append({ from, to });
        return route;
    }

    // 索引里没有非英语直连，互译一律经英语中转
    const QString en = QStringLiteral("en");
    if (from != en && to != en &&
        directionExists(knownPairs, from, en) &&
        directionExists(knownPairs, en, to)) {
        route.kind = RouteKind::Pivot;
        route.hops.append({ from, en });
        route.hops.append({ en, to });
        return route;
    }

    route.kind = RouteKind::Unsupported;
    return route;
}

QString languageCodeForDisplayName(const QString& name) {
    return kDisplayNameToCode().value(name, QString());
}

QString displayNameForLanguageCode(const QString& code) {
    return kCodeToDisplayName().value(code, QString());
}

const QList<CloudTargetLanguage>& cloudTargetLanguages() {
    static const QList<CloudTargetLanguage> list{
        { "en", "英文", QT_TRANSLATE_NOOP("MainWin", "English") },
        { "ja", "日文", QT_TRANSLATE_NOOP("MainWin", "Japanese") },
        { "ko", "韩文", QT_TRANSLATE_NOOP("MainWin", "Korean") },
        { "zh-Hant", "繁体中文", QT_TRANSLATE_NOOP("MainWin", "Traditional Chinese") },
        { "zh-Hans", "简体中文", QT_TRANSLATE_NOOP("MainWin", "Simplified Chinese") },
        { "yue", "粤语", QT_TRANSLATE_NOOP("MainWin", "Cantonese") },
        { "fr", "法文", QT_TRANSLATE_NOOP("MainWin", "French") },
        { "de", "德文", QT_TRANSLATE_NOOP("MainWin", "German") },
        { "es", "西班牙文", QT_TRANSLATE_NOOP("MainWin", "Spanish") },
        { "pt", "葡萄牙文", QT_TRANSLATE_NOOP("MainWin", "Portuguese") },
        { "it", "意大利文", QT_TRANSLATE_NOOP("MainWin", "Italian") },
        { "ru", "俄文", QT_TRANSLATE_NOOP("MainWin", "Russian") },
        { "ar", "阿拉伯文", QT_TRANSLATE_NOOP("MainWin", "Arabic") },
        { "hi", "印地文", QT_TRANSLATE_NOOP("MainWin", "Hindi") },
        { "th", "泰文", QT_TRANSLATE_NOOP("MainWin", "Thai") },
        { "vi", "越南文", QT_TRANSLATE_NOOP("MainWin", "Vietnamese") },
        { "id", "印尼文", QT_TRANSLATE_NOOP("MainWin", "Indonesian") },
        { "ms", "马来文", QT_TRANSLATE_NOOP("MainWin", "Malay") },
        { "tl", "菲律宾文", QT_TRANSLATE_NOOP("MainWin", "Filipino") },
        { "tr", "土耳其文", QT_TRANSLATE_NOOP("MainWin", "Turkish") },
        { "nl", "荷兰文", QT_TRANSLATE_NOOP("MainWin", "Dutch") },
        { "pl", "波兰文", QT_TRANSLATE_NOOP("MainWin", "Polish") },
        { "uk", "乌克兰文", QT_TRANSLATE_NOOP("MainWin", "Ukrainian") },
        { "cs", "捷克文", QT_TRANSLATE_NOOP("MainWin", "Czech") },
        { "sk", "斯洛伐克文", QT_TRANSLATE_NOOP("MainWin", "Slovak") },
        { "hu", "匈牙利文", QT_TRANSLATE_NOOP("MainWin", "Hungarian") },
        { "ro", "罗马尼亚文", QT_TRANSLATE_NOOP("MainWin", "Romanian") },
        { "bg", "保加利亚文", QT_TRANSLATE_NOOP("MainWin", "Bulgarian") },
        { "hr", "克罗地亚文", QT_TRANSLATE_NOOP("MainWin", "Croatian") },
        { "sr", "塞尔维亚文", QT_TRANSLATE_NOOP("MainWin", "Serbian") },
        { "el", "希腊文", QT_TRANSLATE_NOOP("MainWin", "Greek") },
        { "sv", "瑞典文", QT_TRANSLATE_NOOP("MainWin", "Swedish") },
        { "da", "丹麦文", QT_TRANSLATE_NOOP("MainWin", "Danish") },
        { "nb", "挪威文", QT_TRANSLATE_NOOP("MainWin", "Norwegian") },
        { "fi", "芬兰文", QT_TRANSLATE_NOOP("MainWin", "Finnish") },
        { "he", "希伯来文", QT_TRANSLATE_NOOP("MainWin", "Hebrew") },
        { "fa", "波斯文", QT_TRANSLATE_NOOP("MainWin", "Persian") },
        { "bn", "孟加拉文", QT_TRANSLATE_NOOP("MainWin", "Bengali") },
        { "ta", "泰米尔文", QT_TRANSLATE_NOOP("MainWin", "Tamil") },
        { "ur", "乌尔都文", QT_TRANSLATE_NOOP("MainWin", "Urdu") },
    };
    return list;
}

QString promptNameForStored(const QString& stored) {
    for (const CloudTargetLanguage& l : cloudTargetLanguages()) {
        if (stored == QLatin1String(l.code)) return QString::fromUtf8(l.zhName);
    }
    return stored;
}

QString languageCodeForStored(const QString& stored) {
    const QString code = languageCodeForDisplayName(stored);
    if (!code.isEmpty()) return code;
    for (const CloudTargetLanguage& l : cloudTargetLanguages()) {
        if (stored == QString::fromUtf8(l.zhName)) return QString::fromLatin1(l.code);
    }
    return stored;
}

QString resolveSourceCode(const QString& configured, const QString& recognitionCode) {
    if (!configured.isEmpty() && configured != QStringLiteral("auto")) return configured;
    if (recognitionCode == QStringLiteral("zh")) return QStringLiteral("zh-Hans");
    if (recognitionCode == QStringLiteral("yue")) return QStringLiteral("zh-Hant");
    return recognitionCode;
}
