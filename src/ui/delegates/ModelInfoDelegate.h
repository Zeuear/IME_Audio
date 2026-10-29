#pragma once
#include <QStyledItemDelegate>

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

    using QStyledItemDelegate::QStyledItemDelegate;

    static Columns columns(const QRect& row);
    static void drawBars(QPainter* p, const QRect& area, int level, const QPalette& pal);

    QSize sizeHint(const QStyleOptionViewItem& opt, const QModelIndex& index) const override;
    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& index) const override;
    bool helpEvent(QHelpEvent* e, QAbstractItemView* view, const QStyleOptionViewItem& opt,
                   const QModelIndex& index) override;
};
