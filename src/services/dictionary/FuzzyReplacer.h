#pragma once
#include <QList>
#include <QString>

#include "services/hotword/HotwordSelector.h"
#include "services/hotword/PinyinTable.h"

// @brief 对没有热词偏置的识别模型做事后模糊替换：把句子里读音或拼写和词典词条相近的片段换成正确写法。
class FuzzyReplacer {
public:
    static constexpr double kMinPinyinScore = 85.0;
    static constexpr double kMinLatinScore = 88.0;
    static constexpr int kMinLatinLength = 6;
    static constexpr int kMinPinyinUnits = 2;

    explicit FuzzyReplacer(const PinyinTable* table = nullptr);

    void setEntries(const QList<HotwordSelector::Entry>& entries);
    int entryCount() const { return int(m_entries.size()); }
    QString apply(const QString& text) const;

private:
    struct Form {
        std::u32string pinyin;
        QString latin;
        int latinWords = 0;
    };
    struct Entry {
        QString correct;
        QList<Form> forms;
    };
    struct Match {
        int start = 0;
        int end = 0;
        double score = 0;
        QString replacement;
    };

    Form compile(const QString& text) const;
    std::u32string pinyinOf(const QString& text, QList<int>* positions = nullptr) const;
    void matchPinyin(const Entry& entry, const std::u32string& view, const QList<int>& positions, const QString& text, QList<Match>& out) const;
    void matchLatin(const Entry& entry, const QString& text, QList<Match>& out) const;

    const PinyinTable* m_table;
    QList<Entry> m_entries;
};
