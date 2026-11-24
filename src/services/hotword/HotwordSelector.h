#pragma once
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include "PinyinTable.h"

// @brief 按读音从词典里挑出和这句话最相关的热词。纯逻辑，便于单测。
//
// 为什么需要：Qwen3-ASR 这类 LLM 型识别器的热词写在提示词里，词典几百条全塞进去会超出序列长度（识别直接返回空），
// 就算只塞前几十条，也没有针对性。做法是借实时字幕已经识别出的这句话（专有名词常听成同音字），
// 按读音找出词典里"听起来像"的词，只把这几个交给离线识别器：多解码一次音频的代价没有，只多一次文本匹配。
//
// 匹配方式：把文字拆成"读音单元"（汉字 → 拼音编号，连续英文数字 → 一个小写词），
// 在文字里滑动与词条等长（±1）的窗口，算带权编辑距离。权重：读音相同 0；zh/z、ch/c、sh/s、前后鼻音、n/l 之差 0.3；其余 1。
// 词典有几千条时，先用"词条里至少要有几个单元在句子里出现过"粗筛，绝大多数词条不用进编辑距离。
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

    // 按相似度从高到低，相同按词典顺序；text 为空返回空表
    QStringList select(const QString& text, int maxWords = kDefaultMax, double minScore = kDefaultMinScore) const;

    // 暴露给单测
    struct Unit {
        QString latin;              // 非空表示英文词（已小写），此时下面的编号无效
        int sid = -1;               // 拼音编号（多音字取主读音）
        int nid = -1;               // 抹平发音混淆后的编号
        QVector<int> altSid;        // 多音字的其余读音；比较时任一读音对得上就算
        QVector<int> altNid;
        bool isLatin() const { return !latin.isEmpty(); }
    };
    QVector<Unit> units(const QString& text) const;
    double similarity(const QVector<Unit>& term, const QVector<Unit>& text) const;

private:
    struct Compiled {
        QString text;
        QVector<QVector<Unit>> forms;    // 正确写法 + 各听成的写法
    };

    double unitCost(const Unit& a, const Unit& b) const;
    // 粗筛：这个词条至少要有多少单元在句子里出现过，才可能达到 minScore
    bool mayMatch(const QVector<Unit>& form, const QSet<int>& textNids, bool textHasLatin, double minScore) const;

    const PinyinTable* m_table;
    QVector<Compiled> m_entries;
};
