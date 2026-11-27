#pragma once
#include <QString>
#include <QStringList>

#include "services/TextPolishService.h"

namespace TextPolishPrompts {

struct PolishInstructionParts {
    QString style;
    QString langInstruction;
    QString dictInstruction;
    QString vocabInstruction;
    QString customInstruction;
};

bool hasTranslationTarget(const QString& targetLang);

PolishInstructionParts buildPolishInstructionParts(const TextPolishService::RequestParams& params);

QString styleInstruction(const QString& style);

// 只翻译请求的系统提示词：不润色、只输出译文；基础部分可在 prompts/ 目录里改
using GlossaryPairs = QList<QPair<QString, QString>>;

QString glossaryTerms(const GlossaryPairs& glossary, const QString& text);

QString translateOnlyPrompt(const QString& targetLang, const GlossaryPairs& glossary = {}, const QString& text = {});

QStringList buildFewshotMessages(const QString& fewshotText);

QString loadSystemPrompt(const QString& filename, const QString& defaultPrompt);

QString autocompleteApiUrl(const QString& input);

// 去掉思考型模型（Qwen3 / Gemma 4 等）回复里的思考段，只留最终答案；
// 否则 <think>…</think> 会原样打进前台窗口
QString stripThinking(const QString& reply);

// 翻译专用模型（Index-Translate、Hy-MT 等）：只按训练时的单条 user 消息格式翻译，不能拿来润色
bool isTranslationModel(const QString& model);

QString promptLanguageName(const QString& storedTargetLang);

// 翻译专用模型的单条 user 消息（Index-Translate 模型卡给出的格式）
QString translationModelPrompt(const QString& storedTargetLang, const QString& text, const GlossaryPairs& glossary = {});

}
