#pragma once

#include "snippets/SnippetTypes.h"
#include "snippets/SnippetStore.h"
#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>
#include <QColor>
#include <QHash>
#include <QPixmap>
#include <unordered_map>

// Qt 多级树形模型（对应原 ManagerApp 的多级彩色卡片树）
// L1 主表（蓝）/ L2 Sheet（绿）/ L3 @@分组（橙）。每个文件夹的子节点顺序：先子文件夹，后条目。
// 增删移动节点后必须调用 refresh()，模型内部缓存的指针此时才会重建；只改文字用 nodeChanged()。
class SnippetTreeModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum Roles {
        NodeIdRole   = Qt::UserRole + 10,   // QString 节点 id
        IsFolderRole = Qt::UserRole + 11,   // bool
        LevelRole    = Qt::UserRole + 12,   // int 1..4（条目为 4）
        SearchRole   = Qt::UserRole + 13,   // QString 小写检索串（供 QSortFilterProxyModel）
        TextRole     = Qt::UserRole + 14,   // QString 条目完整内容（委托自己换行）
        KeywordRole  = Qt::UserRole + 15,   // QString 快捷码
        CountRole    = Qt::UserRole + 16,   // int 目录下的话术数
        HasImageRole = Qt::UserRole + 17    // bool 条目是否配了图（缩略图读不出来时委托画占位）
    };

    explicit SnippetTreeModel(SnippetStore* store, QObject* parent = nullptr);
    ~SnippetTreeModel() override;

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    SnippetStore* store() const { return m_store; }
    void refresh();
    // 只改了某个节点的文字/配图（树结构、指针都没变）：发 dataChanged，保留视图的展开、选中和滚动位置
    void nodeChanged(const QString& nodeId);

    QModelIndex indexByNodeId(const QString& nodeId) const;
    QString nodeIdFromIndex(const QModelIndex& index) const;
    bool isFolder(const QModelIndex& index) const;
    int nodeLevel(const QModelIndex& index) const;
    QString nodeTitle(const QModelIndex& index) const;

    static QColor levelColor(int level);
    static QColor levelBg(int level);
    static QString levelBadge(int level);

private:
    struct Info {
        bool isFolder = true;
        const FolderNode* parent = nullptr;   // nullptr = 根级
        int row = 0;
        int count = 0;       // 文件夹：其下话术总数（rebuild 时算好，paint 时不再递归）
        QString search;      // 小写检索串，供过滤用
    };

    SnippetStore* m_store;
    std::unordered_map<const void*, Info> m_info;
    QHash<QString, const void*> m_idToPtr;
    // key: 图片路径 + 修改时间；跨 refresh 保留，改文字不用重读所有图
    mutable QHash<QString, QPixmap> m_thumbCache;

    QPixmap thumbnailFor(const SnippetItem& item) const;
    QString imageFileFor(const SnippetItem& item) const;

    void rebuild();
    int rebuildFolder(const FolderNode* f);
    const Info* infoOf(const void* ptr) const;
};
