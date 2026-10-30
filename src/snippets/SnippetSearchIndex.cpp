#include "snippets/SnippetSearchIndex.h"
#include <algorithm>

namespace {

// 英文快捷码要按整词匹配（"hi" 不应命中 "this"）；中日韩文字之间没有空格，仍按子串匹配
bool containsKeyword(const QString& text, const QString& keyword) {
    int from = 0;
    while (true) {
        const int pos = text.indexOf(keyword, from, Qt::CaseInsensitive);
        if (pos < 0) return false;
        auto isWordChar = [](QChar c) { return c.unicode() < 128 && c.isLetterOrNumber(); };
        const bool leftOk = pos == 0 || !isWordChar(keyword.front()) || !isWordChar(text[pos - 1]);
        const int end = pos + keyword.size();
        const bool rightOk = end >= text.size() || !isWordChar(keyword.back()) || !isWordChar(text[end]);
        if (leftOk && rightOk) return true;
        from = pos + 1;
    }
}

} // namespace

QVector<SnippetSearchIndex::Hit> SnippetSearchIndex::search(FolderList& roots, const QString& query, bool allWhenEmpty) {
    QVector<Hit> results;
    const QString q = query.trimmed().toLower();
    if (q.isEmpty() && !allWhenEmpty) return results;
    for (const Entry& e : entries(roots)) {
        if (q.isEmpty() || e.haystack.contains(q)) results.push_back({ e.item, e.trail });
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

const QVector<SnippetSearchIndex::Entry>& SnippetSearchIndex::entries(FolderList& roots) {
    if (!m_valid) {
        m_entries.clear();
        build(roots, QStringList());
        m_valid = true;
    }
    return m_entries;
}

// 遍历顺序与树一致：先本文件夹的条目，再子文件夹
void SnippetSearchIndex::build(FolderList& list, const QStringList& trail) {
    for (auto& folder : list) {
        QStringList next = trail;
        next << folder->title;
        const QString trailStr = next.join(" / ");
        const QString trailLower = trailStr.toLower();
        for (auto& item : folder->items) {
            Entry e;
            e.item = &item;
            e.keyword = item.keyword.trimmed();
            e.trail = trailStr;
            // 用换行隔开各字段，避免查询词横跨两个字段误命中
            e.haystack = (item.keyword + '\n' + item.title + '\n' + item.text + '\n' + trailLower).toLower();
            m_entries.push_back(std::move(e));
        }
        build(folder->children, next);
    }
}
