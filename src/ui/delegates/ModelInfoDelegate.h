#pragma once
#include <QStyledItemDelegate>
#include "ui/models/ModelDownloadTracker.h"

// @brief
// 模型列表的行绘制：单选圈 + 名称/语言 + 准确度/速度信号格 + 特性标签。
// 表头与行共用 columns() 的列划分，保证对齐。
class ModelInfoDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    struct Columns {
        QRect radio;
        QRect name;
        QRect accuracy;
        QRect speed;
        QRect features;
    };

    // 行里"取消 / 重试"这个可点的区域；点到别处只是选中模型
    enum class Action { None, Cancel, Retry };

    using QStyledItemDelegate::QStyledItemDelegate;

    static Columns columns(const QRect& row);
    // 操作区放在"特性"那一列的位置，行高不变；行内进度条画在行底边
    static QRect actionRect(const QRect& row);
    // 下载中点操作区是取消，失败后点操作区是重试；解压中不能取消，没有任务时那一格不是按钮
    static Action actionAt(const QRect& row, const QPoint& pos, ModelDownloadTracker::State state);
    static void drawBars(QPainter* p, const QRect& area, int level, const QPalette& pal);

    QSize sizeHint(const QStyleOptionViewItem& opt, const QModelIndex& index) const override;
    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& index) const override;
    bool helpEvent(QHelpEvent* e, QAbstractItemView* view, const QStyleOptionViewItem& opt,
                   const QModelIndex& index) override;
};
