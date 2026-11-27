#pragma once
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include "PinyinTable.h"

// @brief 按读音从词典里挑出和这句话最相关的热词。纯逻辑，便于单测。
// 为什么：LLM 型识别器的热词写在提示词里，全塞会超出序列长度；借实时字幕已识别的这句话，按读音找出词典里“听起来像”的词，只把这几个交给离线识别器。
// 匹配：汉字 → 拼音编号、连续英文数字 → 小写词，滑动与词条等长（±1）的窗口算带权编辑距离（读音相同 0；zh/z、ch/c、sh/s、前后鼻音、n/l 之差 0.3；其余 1）；词典大时先按“至少出现几个单元”粗筛。
class HotwordSelector
{
public:
    struct Entry {
        QString text;          // 正确写法，就是要交给识别器的热词
        QStringList heard;     // 常听成的写法；文字里出现它们同样说明这个词在场
    };

    // 默认一句最多挑 8 个：提示词里每个词约占 3 个 token，8 个词不到序列长度的 1/20
    static constexpr int kDefaultMax = 8;
    // 相似度（1 = 读音完全一致）达到这个值才算命中。基准测试里 0.6 在 225 条词典上没有误选干扰词
    static constexpr double kDefaultMinScore = 0.6;

    explicit HotwordSelector(const PinyinTable* table = nullptr);

    void setEntries(const QList<Entry>& entries);
    int entryCount() const { return int(m_entries.size()); }

    QStringList select(const QString& text, int maxWords = kDefaultMax, double minScore = kDefaultMinScore) const;

    struct Unit {
        QString latin;              // 非空表示英文词（已小写），此时下面的编号无效
        int sid = -1;
        int nid = -1;
        QVector<int> altSid;        // 多音字的其余读音；比较时任一读音对得上就算
        QVector<int> altNid;
        bool isLatin() const { return !latin.isEmpty(); }
    };
    QVector<Unit> units(const QString& text) const;
    double similarity(const QVector<Unit>& term, const QVector<Unit>& text) const;

private:
    struct Compiled {
        QString text;
        QVector<QVector<Unit>> forms;
    };

    double unitCost(const Unit& a, const Unit& b) const;
    // 粗筛：这个词条至少要有多少单元在句子里出现过，才可能达到 minScore
    bool mayMatch(const QVector<Unit>& form, const QSet<int>& textNids, bool textHasLatin, double minScore) const;

    const PinyinTable* m_table;
    QVector<Compiled> m_entries;
};
