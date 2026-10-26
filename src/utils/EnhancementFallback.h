#pragma once
#include <QString>

// @brief 润色阶段结束后的文本决策
// 润色成功用润色结果；失败但有识别原文时回退原文并标记；连原文都没有才算失败。
struct EnhancementOutcome {
    QString text;
    bool usedFallback = false;
    bool success = false;
};

inline EnhancementOutcome decideEnhancementOutcome(bool polishOk,
                                                   const QString& original,
                                                   const QString& polished) {
    if (polishOk) return { polished, false, true };
    if (!original.isEmpty()) return { original, true, true };
    return { {}, false, false };
}
