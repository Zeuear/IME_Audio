#pragma once
#include <QStyledItemDelegate>

// @brief
// 文件任务卡片的绘制：方块卡片 + 文件名 + 状态文字 + 进度条 + 状态图标。
// 颜色取自当前 palette（跟随 light/gray/dark 主题），仅成功/失败使用固定的语义色。
class FileTaskDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};
