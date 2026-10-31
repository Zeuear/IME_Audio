#pragma once
#include <QRect>
#include <QHash>
#include <QStyledItemDelegate>

class SnippetStore;

// @brief
// 话术树一行的卡片绘制。
//   目录行：展开箭头 + 文件夹图标 + 级别徽章 + 标题 + 话术数，整行铺该级别的半透明底色；
//   条目行：[中]标题 | [外]内容(自动换行) | 快捷码芯片 | 配图缩略图（或“+配图”占位）。
// 颜色取自当前 palette（跟随 light/gray/dark 主题），级别色是固定的语义色。
// 整行只用第 0 列绘制，其余列在视图里隐藏。
class SnippetTreeDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    enum class Zone { None, Title, Text, Keyword, Image };

    explicit SnippetTreeDelegate(SnippetStore* store, QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    Zone zoneAt(const QStyleOptionViewItem& option, const QModelIndex& index, const QPoint& pos) const;

private:
    struct ItemLayout {
        QRect badgeTitle, title, badgeText, text, keyword, image;
        int height = 0;
    };

    struct FolderGeom {
        QFont badgeFont;
        QString badgeText, countText;
        QRect badge, count, title;
    };

    FolderGeom folderGeometry(const QStyleOptionViewItem& option, const QModelIndex& index) const;
    ItemLayout layoutFor(const QRect& row, const QFont& font, const QModelIndex& index) const;
    void paintFolder(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
    void paintItem(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;

    struct MeasureCache {
        QString title, text, fontKey;
        int titleW = 0, textW = 0, titleLines = 0, textLines = 0;
        int titleH = 0, textH = 0;
    };
    mutable QHash<quintptr, MeasureCache> m_measureCache;

    SnippetStore* m_store;
};
