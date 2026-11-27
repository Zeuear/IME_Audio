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

// Qt 多级树形模型：L1 主表 / L2 Sheet / L3 @@分组；子节点先子文件夹后条目。增删移动节点后必须 refresh() 重建指针缓存；只改文字用 nodeChanged()。
class SnippetTreeModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum Roles {
        NodeIdRole   = Qt::UserRole + 10,
        IsFolderRole = Qt::UserRole + 11,
        LevelRole    = Qt::UserRole + 12,
        SearchRole   = Qt::UserRole + 13,
        TextRole     = Qt::UserRole + 14,
        KeywordRole  = Qt::UserRole + 15,
        CountRole    = Qt::UserRole + 16,
        HasImageRole = Qt::UserRole + 17
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
        const FolderNode* parent = nullptr;
        int row = 0;
        int count = 0;
        QString search;
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
