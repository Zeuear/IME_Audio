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
