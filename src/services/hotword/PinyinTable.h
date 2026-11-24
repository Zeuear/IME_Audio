#pragma once
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

// @brief 汉字 → 无声调拼音。热词按读音模糊匹配（HotwordSelector）用。
// 数据是 :/pinyin_table.txt（tools/gen_pinyin_table.py 生成，约 2.7 万字），首次使用时解析一次，不依赖任何第三方库和词典目录。
// 多音字不做词义消歧（那需要词典），而是把所有读音都留着，匹配时任一读音对得上就算：
// 人名里的生僻读音（隗 wěi、缪 miào）靠这个，单取最常用读音会整条词条匹配不上。
// 主读音按码点直接索引（基本区加扩展 A），查一个字就是一次下标访问；其余读音只占少数字，放哈希里。
//
// 拼音在内部是整数编号（syllableId），"抹平 zh/z、ch/c、sh/s、前后鼻音、n/l 之差"后的形式也是编号（normId）：
// 匹配时要比较成千上万次，整数比较比字符串快一个数量级。
class PinyinTable
{
public:
    // 从内置资源加载；资源缺失时是空表（所有汉字都查不到，选词退化为只匹配英文词）
    static const PinyinTable& builtin();
    // 每行 "拼音 汉字串"（多音字的其余读音行以 "+" 开头），供内置资源和单元测试共用
    static PinyinTable fromText(const QString& text);

    // 最常用读音的编号；不是汉字、或没收录时返回 -1
    int syllableId(QChar ch) const;
    // 其余读音的编号（不含主读音），大多数字没有
    QVector<int> alternateIds(QChar ch) const;
    // 抹平常见发音混淆后的编号：编号相同 = 听起来容易混
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
    QVector<int> m_normIds;                    // 拼音编号 → 抹平后的编号
    QVector<qint32> m_index;                   // 码点 - kFirst → 拼音编号 + 1，0 表示没收录
    QHash<ushort, QVector<int>> m_alternates;
    int m_chars = 0;
};
