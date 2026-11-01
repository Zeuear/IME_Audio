#include "snippets/SnippetSearchIndex.h"
#include <algorithm>

namespace {

// 只留字母和数字（转小写），并记下每个字符在原文里的位置
QString stripToAlnum(const QString& s, QVector<int>* origIndex = nullptr) {
    QString out;
    for (int i = 0; i < s.size(); ++i) {
        if (!s[i].isLetterOrNumber()) continue;
        out += s[i].toLower();
        if (origIndex) origIndex->push_back(i);
    }
    return out;
}

// 识别结果会在词中间插标点或空格（如“报，价”、流式字幕里的停顿），所以比较时先去掉标点；
// 英文的词边界仍按原文判断，避免 quote 命中 quotes
bool containsKeyword(const QString& text, const QString& keyword) {
    const QString key = stripToAlnum(keyword);
    if (key.isEmpty()) return false;
    QVector<int> origIndex;
    const QString plain = stripToAlnum(text, &origIndex);
    auto isWordChar = [](QChar c) { return c.unicode() < 128 && c.isLetterOrNumber(); };
    int from = 0;
    while (true) {
        const int pos = plain.indexOf(key, from);
        if (pos < 0) return false;
        const int start = origIndex[pos];
        const int end = origIndex[pos + key.size() - 1] + 1;
        const bool leftOk = start == 0 || !isWordChar(key.front()) || !isWordChar(text[start - 1]);
        const bool rightOk = end >= text.size() || !isWordChar(key.back()) || !isWordChar(text[end]);
        if (leftOk && rightOk) return true;
        from = pos + 1;
    }
}

}

QVector<SnippetSearchIndex::Hit> SnippetSearchIndex::search(FolderList& roots, const QString& query, bool allWhenEmpty) {
    QVector<Hit> results;
    const QString q = query.trimmed().toLower();
    if (q.isEmpty() && !allWhenEmpty) return results;
    for (const Entry& e : entries(roots)) {
        if (q.isEmpty() || e.haystack.contains(q)) results.push_back({ e.item, e.trail, e.trailNames, e.trailKeys });
    }
    return results;
}

QVector<SnippetItem*> SnippetSearchIndex::matchKeywords(FolderList& roots, const QString& spokenText, int limit) {
    QVector<SnippetItem*> hits;
    if (spokenText.trimmed().isEmpty()) return hits;
    for (const Entry& e : entries(roots)) {
        if (!e.keyword.isEmpty() && containsKeyword(spokenText, e.keyword)) hits.push_back(e.item);
    }
    std::stable_sort(hits.begin(), hits.end(),
                     [](const SnippetItem* a, const SnippetItem* b) { return a->useCount > b->useCount; });
    if (hits.size() > limit) hits.resize(limit);
    return hits;
}

bool SnippetSearchIndex::isExactKeyword(const QString& spokenText, const QString& keyword) {
    auto normalized = [](const QString& s) {
        QString out;
        for (QChar c : s) {
            if (c.isLetterOrNumber()) out += c.toLower();
        }
        return out;
    };
    const QString key = normalized(keyword);
    return !key.isEmpty() && normalized(spokenText) == key;
}

const QVector<SnippetSearchIndex::Entry>& SnippetSearchIndex::entries(FolderList& roots) {
    if (!m_valid) {
        m_entries.clear();
        build(roots, QStringList(), QStringList());
        m_valid = true;
    }
    return m_entries;
}

// 遍历顺序与树一致：先本文件夹的条目，再子文件夹
void SnippetSearchIndex::build(FolderList& list, const QStringList& names, const QStringList& keys) {
    for (auto& folder : list) {
        QStringList next = names;
        next << folder->title;
        // 同名目录靠 id 区分；id 缺失（旧数据）时退回节点地址，保证同一次索引内唯一
        QStringList nextKeys = keys;
        nextKeys << (folder->id.isEmpty() ? QString::number(reinterpret_cast<quintptr>(folder.get())) : folder->id);
        const QString trailStr = next.join(" / ");
        const QString trailLower = trailStr.toLower();
        for (auto& item : folder->items) {
            Entry e;
            e.item = &item;
            e.keyword = item.keyword.trimmed();
            e.trail = trailStr;
            e.trailNames = next;
            e.trailKeys = nextKeys;
            // 用换行隔开各字段，避免查询词横跨两个字段误命中
            e.haystack = (item.keyword + '\n' + item.title + '\n' + item.text + '\n' + trailLower).toLower();
            m_entries.push_back(std::move(e));
        }
        build(folder->children, next, nextKeys);
    }
}
