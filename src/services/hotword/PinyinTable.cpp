#include "PinyinTable.h"

#include <QFile>

#include "utils/Logger.h"

namespace {

// 抹平常见的发音混淆：翘舌/平舌、前后鼻音、n/l 不分
QString normalizeSyllable(QString s)
{
    s.replace(QStringLiteral("zh"), QStringLiteral("z"));
    s.replace(QStringLiteral("ch"), QStringLiteral("c"));
    s.replace(QStringLiteral("sh"), QStringLiteral("s"));
    s.replace(QStringLiteral("ng"), QStringLiteral("n"));
    s.replace(QLatin1Char('l'), QLatin1Char('n'));
    return s;
}

} // namespace

const PinyinTable& PinyinTable::builtin()
{
    static const PinyinTable table = []() {
        QFile f(QStringLiteral(":/pinyin_table.txt"));
        if (!f.open(QIODevice::ReadOnly)) {
            LOG_WARN("Pinyin table resource missing, hotword matching limited to latin words");
            return PinyinTable();
        }
        return fromText(QString::fromUtf8(f.readAll()));
    }();
    return table;
}

int PinyinTable::intern(const QString& syllable)
{
    const auto it = m_syllableIds.constFind(syllable);
    if (it != m_syllableIds.constEnd()) return it.value();
    const int id = int(m_syllables.size());
    m_syllables << syllable;
    m_syllableIds.insert(syllable, id);
    return id;
}

PinyinTable PinyinTable::fromText(const QString& text)
{
    PinyinTable t;
    t.m_index.fill(0, kLast - kFirst + 1);
    for (const QStringView line : QStringView(text).split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        // 以 "+" 开头的行是多音字的其余读音
        const bool extra = line.startsWith(QLatin1Char('+'));
        const QStringView body = extra ? line.mid(1) : line;
        const int space = body.indexOf(QLatin1Char(' '));
        if (space <= 0) continue;
        const int id = t.intern(body.left(space).toString());
        for (const QChar ch : body.mid(space + 1)) {
            const ushort u = ch.unicode();
            if (u < kFirst || u > kLast) continue;
            if (extra) {
                t.m_alternates[u].push_back(id);
            } else {
                if (t.m_index[u - kFirst] == 0) ++t.m_chars;
                t.m_index[u - kFirst] = id + 1;
            }
        }
    }
    // 抹平后相同的拼音共用一个编号
    QHash<QString, int> normIds;
    t.m_normIds.resize(t.m_syllables.size());
    for (int i = 0; i < t.m_syllables.size(); ++i) {
        const QString n = normalizeSyllable(t.m_syllables[i]);
        t.m_normIds[i] = normIds.value(n, -1);
        if (t.m_normIds[i] < 0) t.m_normIds[i] = normIds[n] = int(normIds.size());
    }
    return t;
}

int PinyinTable::syllableId(QChar ch) const
{
    const ushort u = ch.unicode();
    if (u < kFirst || u > kLast || m_index.isEmpty()) return -1;
    return m_index[u - kFirst] - 1;
}

QVector<int> PinyinTable::alternateIds(QChar ch) const
{
    return m_alternates.value(ch.unicode());
}

QString PinyinTable::syllable(QChar ch) const
{
    const int id = syllableId(ch);
    return id < 0 ? QString() : m_syllables[id];
}

QStringList PinyinTable::alternates(QChar ch) const
{
    QStringList out;
    for (const int id : alternateIds(ch)) out << m_syllables[id];
    return out;
}
