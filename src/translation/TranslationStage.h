#pragma once
#include <QString>
#include "LanguageRouting.h"

// @brief 是否进入本地翻译阶段；纯逻辑，便于单测
// from / to 是语言代码；配置里存的目标语言可能是中文显示名，这里统一转换
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
    // 文件转录结果不进入注入链路，也不做本地翻译
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

// 翻译结束后的文本：成功用译文，失败回退翻译前文字并标记
struct TranslationOutcome {
    QString text;
    bool usedFallback = false;
};

inline TranslationOutcome decideTranslationOutcome(bool ok, const QString& translated,
                                                   const QString& original) {
    if (ok) return { translated, false };
    return { original, true };
}
