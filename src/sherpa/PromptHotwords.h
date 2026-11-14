#pragma once

#include <QString>

// @brief 把词典热词（每行一个）整理成 Qwen3-ASR / FunASR-Nano 用的逗号分隔串
//
// 这两类模型的热词是写进大模型提示词开头的，和音频共用解码器的总长度（Qwen3-ASR 默认 512）。
// 热词太多会把音频挤掉，sherpa-onnx 只能截断音频，结果就是丢字；
// 所以按词典顺序取，估算的 token 数到预算就停，其余丢弃。
class PromptHotwords {
public:
    // sherpa-onnx 在热词超过 48 个 token 时就开始警告长度吃紧，取同一个值
    static constexpr int kMaxTokens = 48;

    struct Result {
        QString csv;        // "词1,词2,..."，没有可用热词时为空
        int used = 0;       // 写进提示词的词数
        int dropped = 0;    // 超出预算没用上的词数
        int tokens = 0;     // 估算占用的 token 数
    };

    static Result build(const QString& lines, int maxTokens = kMaxTokens);

    // 粗估一个词在 Qwen 分词器里的 token 数，宁可估多：
    // 汉字等非 ASCII 字符按 1 字 1 个，连续的 ASCII 字母数字按 4 个字符 1 个，再加 1 个分隔符
    static int estimateTokens(const QString& word);
};
