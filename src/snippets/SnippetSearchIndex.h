#pragma once

#include "snippets/SnippetTypes.h"
#include <QString>
#include <QStringList>
#include <QVector>

// @brief
// 把 QString 小写化、目录路径拼接的开销放在树变动后的首次查询里，而不是每次按键都重算。
// 索引里存的是树内指针：树增删节点后必须调用 invalidate()（SnippetStore::save 会做）。
class SnippetSearchIndex {
public:
    struct Hit {
        SnippetItem* item = nullptr;
        QString trail;
        QStringList trailNames;   // 逐级目录名，显示用；名字里可能含 " / "，不能靠拆 trail 还原
        QStringList trailKeys;
    };

    void invalidate() { m_valid = false; }

    QVector<Hit> search(FolderList& roots, const QString& query, bool allWhenEmpty);

    QVector<SnippetItem*> matchKeywords(FolderList& roots, const QString& spokenText, int limit);

    // 整句话是否就是这个快捷码。识别结果常带句末标点和空格，比较时只看字母和数字
    static bool isExactKeyword(const QString& spokenText, const QString& keyword);

private:
    struct Entry {
        SnippetItem* item = nullptr;
        QString keyword;
        QString haystack;
        QString trail;
        QStringList trailNames;
        QStringList trailKeys;
    };

    const QVector<Entry>& entries(FolderList& roots);
    void build(FolderList& list, const QStringList& names, const QStringList& keys);

    QVector<Entry> m_entries;
    bool m_valid = false;
};
