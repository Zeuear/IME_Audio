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
    Same,      // 源语言等于目标语言，无需翻译
    Direct,    // 已知索引里直接有 from>to
    Pivot,     // 非英语互译，需经英语中转：from->en->to
    Unsupported, // 缺少任意一跳，不支持该方向
};

struct TranslationRoute {
    RouteKind kind = RouteKind::Unsupported;
    QList<TranslationHop> hops;
};

// 计算翻译方向。knownPairs 元素形如 "zh-Hans>en"。
TranslationRoute planRoute(const QString& from, const QString& to,
                           const QSet<QString>& knownPairs);

// "英文" -> "en"；未知显示名返回空字符串。
QString languageCodeForDisplayName(const QString& name);

// "en" -> "英文"；未知代码返回空字符串。
QString displayNameForLanguageCode(const QString& code);

// 配置里存的目标语言：常用五种存中文显示名（会写进云端提示词，不能改），其余直接存语言代码。
// 统一转成语言代码；已经是代码的原样返回。
QString languageCodeForStored(const QString& stored);

// 源语言为 auto 时按识别模型的语言推断；中文按简体、粤语按繁体处理。无法确定返回空。
QString resolveSourceCode(const QString& configured, const QString& recognitionCode);
