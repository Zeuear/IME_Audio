#include "TranscriptLineDelegate.h"
#include "ui/models/TranscriptLineModel.h"
#include <QListView>
#include <QPainter>

namespace {
constexpr int kMinRowHeight = 28;
constexpr int kPadX = 8;
constexpr int kPadY = 5;
constexpr int kTimeWidth = 64;
constexpr int kGutterWidth = 24;   // 不显示时间戳时选行栏的宽度
constexpr int kColumnGap = 16;
constexpr int kTextFlags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap;
}

TranscriptLineDelegate::TranscriptLineDelegate(QListView* view)
    : QStyledItemDelegate(view)
    , m_view(view)
{
}

static int textLeftOffset(bool showTime)
{
    return showTime ? kPadX + kTimeWidth + kPadX : kGutterWidth;
}

QRect TranscriptLineDelegate::gutterRect(const QRect& row) const
{
    return QRect(row.left(), row.top(), textLeftOffset(m_showTime), row.height());
}

QRect TranscriptLineDelegate::textRect(const QRect& row) const
{
    const int left = row.left() + textLeftOffset(m_showTime);
    const int right = row.right() - kPadX;
    return QRect(left, row.top() + kPadY, qMax(1, right - left), row.height() - 2 * kPadY);
}

TranscriptLineDelegate::Hit TranscriptLineDelegate::hitTest(const QRect& row, const QPoint& pos) const
{
    if (!row.contains(pos)) return Hit::None;
    if (gutterRect(row).contains(pos)) return Hit::Gutter;
    // 两栏之间的间隙算作译文栏左边缘以内的部分按位置分：以分隔线为界
    const int mid = (originalRect(row).right() + translationRect(row).left()) / 2;
    return pos.x() <= mid ? Hit::Original : Hit::Translation;
}

int TranscriptLineDelegate::dividerX(int viewportWidth) const
{
    const QRect row(0, 0, viewportWidth, 1);
    return (originalRect(row).right() + translationRect(row).left()) / 2;
}

QRect TranscriptLineDelegate::originalRect(const QRect& row) const
{
    const QRect all = textRect(row);
    const int half = (all.width() - kColumnGap) / 2;
    return QRect(all.left(), all.top(), qMax(1, half), all.height());
}

QRect TranscriptLineDelegate::translationRect(const QRect& row) const
{
    const QRect all = textRect(row);
    const int half = (all.width() - kColumnGap) / 2;
    return QRect(all.right() - half + 1, all.top(), qMax(1, half), all.height());
}

QSize TranscriptLineDelegate::sizeHint(const QStyleOptionViewItem& opt, const QModelIndex& index) const
{
    // QListView 计算行高时 opt.rect 不带宽度，按视口宽度换行
    const int width = m_view->viewport()->width();
    const QRect probe(0, 0, width, kMinRowHeight);
    const QRect left = opt.fontMetrics.boundingRect(QRect(0, 0, originalRect(probe).width(), INT_MAX), kTextFlags,
                                                    index.data().toString());
    const QRect right = opt.fontMetrics.boundingRect(QRect(0, 0, translationRect(probe).width(), INT_MAX), kTextFlags,
                                                     index.data(TranscriptLineModel::TranslationRole).toString());
    return QSize(width, qMax(kMinRowHeight, qMax(left.height(), right.height()) + 2 * kPadY));
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
    p->drawText(originalRect(r), kTextFlags, index.data().toString());
    // 译文用链接色，随 light / gray / dark 主题变化；两栏之间一条淡竖线
    QColor divider = pal.color(QPalette::PlaceholderText);
    divider.setAlpha(60);
    const int dividerX = (originalRect(r).right() + translationRect(r).left()) / 2;
    p->setPen(divider);
    p->drawLine(dividerX, r.top() + kPadY, dividerX, r.bottom() - kPadY);
    p->setPen(pal.color(QPalette::Link));
    p->drawText(translationRect(r), kTextFlags, index.data(TranscriptLineModel::TranslationRole).toString());

    // 选行栏：选中时画一个实心点，提示"这一栏是用来选行的"
    if (selected && !m_showTime) {
        p->setPen(Qt::NoPen);
        p->setBrush(pal.color(QPalette::Highlight));
        p->setRenderHint(QPainter::Antialiasing);
        p->drawEllipse(QPoint(r.left() + kGutterWidth / 2, r.center().y()), 3, 3);
    }
    p->restore();
}
