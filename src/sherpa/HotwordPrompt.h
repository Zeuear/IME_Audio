#pragma once
#include <QSet>
#include <QString>
#include <QStringList>

// @brief 把热词表整理成 LLM 型识别器（Qwen3-ASR、FunASR Nano）要的格式。纯逻辑，便于单测。
// LLM 型模型没有"提高某词得分"的机制，热词作为提示词喂给解码器；词越多越慢、越挤占序列长度。
// 实测 Qwen3-ASR 0.6B：30 个词约 100 token、每句慢约 1 秒，225 个词超出 512 总长而返回空；sherpa 约 48 token 起告警，故上限 10 词 / 40 字符。
namespace HotwordPrompt {

constexpr int kMaxWords = 10;
constexpr int kMaxChars = 40;

inline QString build(const QString& words, int maxWords = kMaxWords, int maxChars = kMaxChars)
{
    QStringList kept;
    QSet<QString> seen;
    int chars = 0;
    for (QString w : words.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        // 词里的逗号会被当成分隔符拆成两个词，换成空格保住原词
        w.replace(QLatin1Char(','), QLatin1Char(' ')).replace(QChar(0xFF0C), QLatin1Char(' '));
        w = w.trimmed();
        if (w.isEmpty() || seen.contains(w)) continue;
        if (kept.size() >= maxWords || chars + w.size() > maxChars) break;
        seen.insert(w);
        chars += w.size();
        kept << w;
    }
    return kept.join(QLatin1Char(','));
}

inline QString build(const QStringList& words, int maxWords = kMaxWords, int maxChars = kMaxChars)
{
    return build(words.join(QLatin1Char('\n')), maxWords, maxChars);
}

inline int count(const QString& csv) { return csv.isEmpty() ? 0 : int(csv.count(QLatin1Char(','))) + 1; }

} // namespace HotwordPrompt
