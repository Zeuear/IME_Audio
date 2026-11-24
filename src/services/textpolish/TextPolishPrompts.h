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

// 是否设置了有效的目标语言（空和"不翻译"都表示没有翻译任务）
bool hasTranslationTarget(const QString& targetLang);

// 根据 params 计算 style/lang/dict/vocab/custom 五个片段
PolishInstructionParts buildPolishInstructionParts(const TextPolishService::RequestParams& params);

// 根据 UI 选择的风格返回系统提示词
QString styleInstruction(const QString& style);

// 只翻译请求的系统提示词：不润色、只输出译文；基础部分可在 prompts/ 目录里改
QString translateOnlyPrompt(const QString& targetLang);

// 解析 fewshot 文本（user:/assistant: 及全角冒号变体）为 [role, content, role, content, ...]
QStringList buildFewshotMessages(const QString& fewshotText);

// 加载/落盘 prompts/ 目录下的系统提示词文件
QString loadSystemPrompt(const QString& filename, const QString& defaultPrompt);

// 补全 API URL：若未以 /v1/chat/completions 结尾，自动补齐
QString autocompleteApiUrl(const QString& input);

// 去掉思考型模型（Qwen3 / Gemma 4 等）回复里的思考段，只留最终答案；
// 否则 <think>…</think> 会原样打进前台窗口
QString stripThinking(const QString& reply);

// 翻译专用模型（Index-Translate、Hy-MT 等）：只按训练时的单条 user 消息格式翻译，不能拿来润色
bool isTranslationModel(const QString& model);

// 配置里的目标语言（"英文"/"简体中文"/语言代码）转成写进提示词的语言名
QString promptLanguageName(const QString& storedTargetLang);

// 翻译专用模型的单条 user 消息（Index-Translate 模型卡给出的格式）
QString translationModelPrompt(const QString& storedTargetLang, const QString& text);

} // namespace TextPolishPrompts
