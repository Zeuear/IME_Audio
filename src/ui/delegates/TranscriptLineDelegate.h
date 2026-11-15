#pragma once
#include <QStyledItemDelegate>

class QListView;

// @brief 实时转录一行：左侧时间（可隐藏），中间整句自动换行，悬停时右侧出现"复制"。
class TranscriptLineDelegate : public QStyledItemDelegate
{
public:
    explicit TranscriptLineDelegate(QListView* view);

    void setShowTime(bool show) { m_showTime = show; }
    void setCopyText(const QString& text) { m_copyText = text; }

    // 绘制和点击判断共用
    static QRect copyRect(const QRect& row);

    QSize sizeHint(const QStyleOptionViewItem& opt, const QModelIndex& index) const override;
    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& index) const override;

private:
    QRect textRect(const QRect& row) const;

    QListView* m_view;
    bool m_showTime = false;
    QString m_copyText;
};
