#pragma once
#include <QStyledItemDelegate>

class QListView;

class TranscriptLineDelegate : public QStyledItemDelegate
{
public:
    explicit TranscriptLineDelegate(QListView* view);

    void setShowTime(bool show) { m_showTime = show; }

    // 点在行内哪个区域
    enum class Hit { None, Gutter, Original, Translation };
    Hit hitTest(const QRect& row, const QPoint& pos) const;
    // 原文 / 译文的矩形
    QRect sideRect(const QRect& row, bool translation) const { return translation ? translationRect(row) : originalRect(row); }

    QSize sizeHint(const QStyleOptionViewItem& opt, const QModelIndex& index) const override;
    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& index) const override;

private:
    // 文字区先扣掉左侧选行栏，再平分成原文、译文两栏
    QRect gutterRect(const QRect& row) const;
    QRect textRect(const QRect& row) const;
    QRect originalRect(const QRect& row) const;
    QRect translationRect(const QRect& row) const;

    QListView* m_view;
    bool m_showTime = false;
};
