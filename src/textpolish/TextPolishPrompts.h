#pragma once
#include <QString>
#include <QStringList>

#include "TextPolishService.h"

namespace TextPolishPrompts {

struct PolishInstructionParts {
    QString style;
    QString langInstruction;
    QString dictInstruction;
    QString vocabInstruction;
    QString customInstruction;
};

// 根据 params 计算 style/lang/dict/vocab/custom 五个片段
PolishInstructionParts buildPolishInstructionParts(const TextPolishService::RequestParams& params);

// 根据 UI 选择的风格返回系统提示词
QString styleInstruction(const QString& style);

// 解析 fewshot 文本（user:/assistant: 及全角冒号变体）为 [role, content, role, content, ...]
QStringList buildFewshotMessages(const QString& fewshotText);

// 加载/落盘 prompts/ 目录下的系统提示词文件
QString loadSystemPrompt(const QString& filename, const QString& defaultPrompt);

// 补全 API URL：若未以 /v1/chat/completions 结尾，自动补齐
QString autocompleteApiUrl(const QString& input);

} // namespace TextPolishPrompts
