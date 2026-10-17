#pragma once
#include <QList>
#include <QString>
#include <QStringList>

struct SubtitleCue {
    qint64 startMs = 0;
    qint64 endMs = 0;
    QString text;
};

inline QString formatSrtTime(qint64 ms)
{
    if (ms < 0) ms = 0;
    const qint64 h = ms / 3600000;
    const int m = static_cast<int>(ms / 60000 % 60);
    const int s = static_cast<int>(ms / 1000 % 60);
    const int milli = static_cast<int>(ms % 1000);
    return QStringLiteral("%1:%2:%3,%4")
        .arg(h, 2, 10, QLatin1Char('0'))
        .arg(m, 2, 10, QLatin1Char('0'))
        .arg(s, 2, 10, QLatin1Char('0'))
        .arg(milli, 3, 10, QLatin1Char('0'));
}

// 预览用时间戳 hh:mm:ss.mmm，与 SRT 只差毫秒分隔符，便于和导出的字幕对照
inline QString formatPreviewTime(qint64 ms)
{
    QString t = formatSrtTime(ms);
    t[t.size() - 4] = QLatin1Char('.');
    return t;
}

// 每段一行、行首带起止时间，只用于界面预览；导出 txt 仍是不带时间戳的纯文本
inline QString buildTimestampedText(const QList<SubtitleCue>& cues)
{
    QStringList out;
    for (const SubtitleCue& cue : cues) {
        const QString text = cue.text.trimmed();
        if (text.isEmpty()) continue;
        const qint64 end = cue.endMs > cue.startMs ? cue.endMs : cue.startMs;
        out << QStringLiteral("[%1 --> %2] %3").arg(formatPreviewTime(cue.startMs), formatPreviewTime(end), text);
    }
    return out.join(QLatin1Char('\n'));
}

inline QString buildSrt(const QList<SubtitleCue>& cues)
{
    QString out;
    int index = 0;
    for (const SubtitleCue& cue : cues) {
        const QStringList lines = cue.text.trimmed().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        if (lines.isEmpty()) continue;
        const qint64 end = cue.endMs > cue.startMs ? cue.endMs : cue.startMs + 1;
        out += QStringLiteral("%1\n%2 --> %3\n%4\n\n")
                   .arg(++index)
                   .arg(formatSrtTime(cue.startMs), formatSrtTime(end), lines.join(QLatin1Char('\n')));
    }
    return out;
}
