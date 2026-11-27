#pragma once

#include <QList>
#include <QSet>
#include <QString>

// @brief 翻译路由与语言显示名与代码互查；纯逻辑，不联网、不写界面。
struct TranslationHop {
    QString from;
    QString to;
};

enum class RouteKind {
    Same,
    Direct,
    Pivot,     // 非英语互译，需经英语中转：from->en->to
    Unsupported,
};

struct TranslationRoute {
    RouteKind kind = RouteKind::Unsupported;
    QList<TranslationHop> hops;
};

TranslationRoute planRoute(const QString& from, const QString& to,
                           const QSet<QString>& knownPairs);

QString languageCodeForDisplayName(const QString& name);

QString displayNameForLanguageCode(const QString& code);

struct CloudTargetLanguage {
    const char* code;
    const char* zhName;
    const char* label;
};
const QList<CloudTargetLanguage>& cloudTargetLanguages();

QString promptNameForStored(const QString& stored);

// 配置里存的目标语言：常用五种存中文显示名（会写进云端提示词，不能改），其余直接存语言代码。
QString languageCodeForStored(const QString& stored);

// 源语言为 auto 时按识别模型的语言推断；中文按简体、粤语按繁体处理。无法确定返回空。
QString resolveSourceCode(const QString& configured, const QString& recognitionCode);
