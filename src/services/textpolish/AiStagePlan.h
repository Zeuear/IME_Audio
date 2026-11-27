#pragma once
#include <QString>

#include "TextPolishPrompts.h"

// @brief 听写结果经过哪些 AI 阶段：润色、翻译各自由开关决定，互不依附。纯逻辑，便于单测。
// 两段都要做时，翻译用的模型就是润色模型（且不是翻译专用模型）则合成一次请求，省一次往返；
// 否则先润色、再把润色结果交给翻译模型。
struct AiStagePlan {
    bool polish = false;
    bool translate = false;
    bool combined = false;
    QString translateModel;
};

inline AiStagePlan planAiStages(bool enhancementOn, const QString& targetLang,
                                bool canPolish, bool canTranslate,
                                const QString& polishModel, const QString& translateModel) {
    AiStagePlan plan;
    plan.translateModel = translateModel.trimmed().isEmpty() ? polishModel : translateModel.trimmed();
    // 翻译专用模型不会润色：主模型选了它就只做翻译，不能把润色提示词丢给它
    plan.polish = enhancementOn && canPolish && !TextPolishPrompts::isTranslationModel(polishModel);
    plan.translate = canTranslate && TextPolishPrompts::hasTranslationTarget(targetLang);
    plan.combined = plan.polish && plan.translate && plan.translateModel == polishModel;
    return plan;
}
