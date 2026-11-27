#pragma once
#include <QStyledItemDelegate>

// @brief 词典配置列表的一行：勾选框（启用）+ 名称 + 绑定语言。
class DictionaryProfileDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override;

    static QRect checkRect(const QRect& rowRect);
};
