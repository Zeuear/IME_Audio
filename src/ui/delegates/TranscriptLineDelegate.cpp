#include "TranscriptLineDelegate.h"
#include "ui/models/TranscriptLineModel.h"
#include <QListView>
#include <QPainter>

namespace {
constexpr int kMinRowHeight = 28;
constexpr int kPadX = 8;
constexpr int kPadY = 5;
constexpr int kTimeWidth = 64;
constexpr int kCopyWidth = 44;
constexpr int kTextFlags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap;
}

TranscriptLineDelegate::TranscriptLineDelegate(QListView* view)
    : QStyledItemDelegate(view)
    , m_view(view)
{
}

QRect TranscriptLineDelegate::copyRect(const QRect& row)
{
    return QRect(row.right() - kPadX - kCopyWidth, row.top(), kCopyWidth, row.height());
}

// 右侧始终给"复制"留出位置：悬停时文字不重新换行，行高不跳
QRect TranscriptLineDelegate::textRect(const QRect& row) const
{
    const int left = row.left() + kPadX + (m_showTime ? kTimeWidth + kPadX : 0);
    const int right = copyRect(row).left() - kPadX;
    return QRect(left, row.top() + kPadY, qMax(1, right - left), row.height() - 2 * kPadY);
}

QSize TranscriptLineDelegate::sizeHint(const QStyleOptionViewItem& opt, const QModelIndex& index) const
{
    // QListView 计算行高时 opt.rect 不带宽度，按视口宽度换行
    const int width = m_view->viewport()->width();
    const QRect text = textRect(QRect(0, 0, width, kMinRowHeight));
    const QRect bound = opt.fontMetrics.boundingRect(QRect(0, 0, text.width(), INT_MAX), kTextFlags,
                                                     index.data().toString());
    return QSize(width, qMax(kMinRowHeight, bound.height() + 2 * kPadY));
}

void TranscriptLineDelegate::paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& index) const
{
    const QPalette& pal = opt.palette;
    const bool hovered = opt.state & QStyle::State_MouseOver;
    const bool selected = opt.state & QStyle::State_Selected;
    const QRect r = opt.rect;
    p->save();
    if (selected || hovered) {
        QColor bg = pal.color(QPalette::Highlight);
        bg.setAlpha(selected ? 70 : 40);
        p->fillRect(r, bg);
    }

    if (m_showTime) {
        p->setPen(pal.color(QPalette::PlaceholderText));
        const QRect timeRect(r.left() + kPadX, r.top() + kPadY, kTimeWidth, kMinRowHeight - 2 * kPadY);
        p->drawText(timeRect, Qt::AlignLeft | Qt::AlignVCenter,
                    index.data(TranscriptLineModel::TimeRole).toString());
    }

    p->setPen(pal.color(QPalette::Text));
    p->drawText(textRect(r), kTextFlags, index.data().toString());

    if (hovered) {
        p->setPen(pal.color(QPalette::Highlight));
        p->drawText(copyRect(r), Qt::AlignCenter, m_copyText);
    }
    p->restore();
}
