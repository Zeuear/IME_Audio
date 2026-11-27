#pragma once
#include <QString>
#include "LanguageRouting.h"

// @brief 是否进入本地翻译阶段；纯逻辑，便于单测
struct TranslationStageDecision {
    bool shouldTranslate = false;
    QString from;
    QString to;
};

inline TranslationStageDecision decideTranslationStage(bool fileMode, bool localEngine,
                                                       const QString& sourceLang,
                                                       const QString& targetLang,
                                                       const QString& recognitionCode = {}) {
    TranslationStageDecision d;
    if (fileMode) return d;
    // 云端引擎的翻译随润色请求一起发出；只有本地引擎才走这里
    if (!localEngine) return d;
    if (targetLang.isEmpty() || targetLang == QStringLiteral("不翻译")) return d;

    d.from = resolveSourceCode(sourceLang, recognitionCode);
    d.to = languageCodeForStored(targetLang);
    if (d.from == d.to) return {};

    d.shouldTranslate = true;
    return d;
}

struct TranslationOutcome {
    QString text;
    bool usedFallback = false;
};

inline TranslationOutcome decideTranslationOutcome(bool ok, const QString& translated,
                                                   const QString& original) {
    if (ok) return { translated, false };
    return { original, true };
}
