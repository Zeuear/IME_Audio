#pragma once
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

// @brief 汉字 → 无声调拼音。热词按读音模糊匹配（HotwordSelector）用。
// 数据是 :/pinyin_table.txt（tools/gen_pinyin_table.py 生成），首次使用时解析一次。
// 多音字不消歧，保留所有读音、任一对得上即算（人名生僻读音如 隗 wěi 靠它）；拼音内部用整数编号（syllableId / 抹平混淆后的 normId），匹配要比较成千上万次。
class PinyinTable
{
public:
    // 从内置资源加载；资源缺失时是空表（所有汉字都查不到，选词退化为只匹配英文词）
    static const PinyinTable& builtin();
    static PinyinTable fromText(const QString& text);

    int syllableId(QChar ch) const;
    QVector<int> alternateIds(QChar ch) const;
    int normId(int syllableId) const { return m_normIds[syllableId]; }

    QString syllable(QChar ch) const;
    QStringList alternates(QChar ch) const;
    int charCount() const { return m_chars; }

private:
    static constexpr ushort kFirst = 0x3400;
    static constexpr ushort kLast = 0x9FFF;

    int intern(const QString& syllable);

    QStringList m_syllables;
    QHash<QString, int> m_syllableIds;
    QVector<int> m_normIds;
    QVector<qint32> m_index;                   // 码点 - kFirst → 拼音编号 + 1，0 表示没收录
    QHash<ushort, QVector<int>> m_alternates;
    int m_chars = 0;
};
