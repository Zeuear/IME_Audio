#include "LanguageRouting.h"

#include <QHash>

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

QString languageCodeForStored(const QString& stored) {
    const QString code = languageCodeForDisplayName(stored);
    return code.isEmpty() ? stored : code;
}

QString resolveSourceCode(const QString& configured, const QString& recognitionCode) {
    if (!configured.isEmpty() && configured != QStringLiteral("auto")) return configured;
    if (recognitionCode == QStringLiteral("zh")) return QStringLiteral("zh-Hans");
    if (recognitionCode == QStringLiteral("yue")) return QStringLiteral("zh-Hant");
    return recognitionCode;
}
