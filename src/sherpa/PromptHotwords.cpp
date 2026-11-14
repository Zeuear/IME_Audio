#include "PromptHotwords.h"

#include <QSet>
#include <QStringList>

int PromptHotwords::estimateTokens(const QString& word)
{
    int tokens = 1;   // 词与词之间的分隔符
    int asciiRun = 0;
    auto flushAscii = [&]() {
        tokens += (asciiRun + 3) / 4;
        asciiRun = 0;
    };
    for (const QChar ch : word) {
        if (ch.unicode() < 0x80 && ch.isLetterOrNumber()) {
            ++asciiRun;
            continue;
        }
        flushAscii();
        if (ch.isSpace()) continue;
        ++tokens;   // 汉字、标点等
    }
    flushAscii();
    return tokens;
}

PromptHotwords::Result PromptHotwords::build(const QString& lines, int maxTokens)
{
    Result r;
    QStringList words;
    QSet<QString> seen;
    for (QString w : lines.split(QLatin1Char('\n'))) {
        // 英文逗号是 sherpa-onnx 的分隔符，词里自带的逗号换成空格，免得一个词被拆开
        w.replace(QLatin1Char(','), QLatin1Char(' '));
        w = w.simplified();
        if (w.isEmpty() || seen.contains(w)) continue;
        seen.insert(w);

        const int cost = estimateTokens(w);
        if (r.dropped > 0 || r.tokens + cost > maxTokens) {
            // 预算用完后一律丢弃，保证用上的是词典里靠前的那些
            ++r.dropped;
            continue;
        }
        r.tokens += cost;
        words << w;
    }
    r.used = words.size();
    r.csv = words.join(QLatin1Char(','));
    return r;
}
