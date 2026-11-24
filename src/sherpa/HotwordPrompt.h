#pragma once
#include <QSet>
#include <QString>
#include <QStringList>

// @brief 把热词表整理成 LLM 型识别器（Qwen3-ASR、FunASR Nano）要的格式。纯逻辑，便于单测。
// 这两类模型没有"提高某词得分"的机制，热词是作为提示词的一部分喂给解码器的：
// 格式是半角逗号分隔的一行；热词越多，提示词越长，每句解码越慢，还会挤占序列长度。
// 实测（Qwen3-ASR 0.6B）：30 个 3~4 字的词约占 100 个 token，每句慢约 1 秒；225 个词超出总长 512，识别直接返回空。
// sherpa 在热词占到约 48 个 token 时就会告警，所以上限按它定：10 个词、40 个字符，宁少勿多。
namespace HotwordPrompt {

constexpr int kMaxWords = 10;
constexpr int kMaxChars = 40;

// words：一行一个（与 sherpa.hotwords 的存法一致）。超出上限的尾部直接丢弃，前面的优先
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

// 已经按相关度排好序的词表：同样套用上限，前面的优先
inline QString build(const QStringList& words, int maxWords = kMaxWords, int maxChars = kMaxChars)
{
    return build(words.join(QLatin1Char('\n')), maxWords, maxChars);
}

inline int count(const QString& csv) { return csv.isEmpty() ? 0 : int(csv.count(QLatin1Char(','))) + 1; }

} // namespace HotwordPrompt
