#include "DictionaryProfileDelegate.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include "ui/models/DictionaryProfileModel.h"

namespace {
constexpr int kRowHeight = 54;
constexpr int kInset = 4;
constexpr int kCheckSize = 16;
constexpr int kCheckLeft = 10;
constexpr int kTextLeft = 36;
}

QSize DictionaryProfileDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const {
    return QSize(0, kRowHeight);
}

QRect DictionaryProfileDelegate::checkRect(const QRect& rowRect) {
    const QRect row = rowRect.adjusted(kInset, 3, -kInset, -3);
    return QRect(row.left() + kCheckLeft, row.center().y() - kCheckSize / 2, kCheckSize, kCheckSize);
}

void DictionaryProfileDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const QPalette& pal = option.palette;
    const QRect row = option.rect.adjusted(kInset, 3, -kInset, -3);
    const bool enabled = index.data(DictionaryProfileModel::EnabledRole).toBool();

    if (option.state & QStyle::State_Selected) {
        QColor fill = pal.color(QPalette::Highlight);
        fill.setAlpha(70);
        painter->setPen(Qt::NoPen);
        painter->setBrush(fill);
        painter->drawRoundedRect(row, 6, 6);
    } else if (option.state & QStyle::State_MouseOver) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(128, 128, 128, 28));
        painter->drawRoundedRect(row, 6, 6);
    }

    const QRect box = checkRect(option.rect);
    if (enabled) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(pal.color(QPalette::Highlight));
        painter->drawRoundedRect(box, 4, 4);
        QPen tick(pal.color(QPalette::HighlightedText), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(tick);
        painter->setBrush(Qt::NoBrush);
        QPainterPath path;
        path.moveTo(box.left() + 4, box.center().y() + 1);
        path.lineTo(box.left() + 7, box.bottom() - 4);
        path.lineTo(box.right() - 3, box.top() + 5);
        painter->drawPath(path);
    } else {
        painter->setPen(QPen(pal.color(QPalette::Mid), 1.5));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(QRectF(box).adjusted(0.75, 0.75, -0.75, -0.75), 4, 4);
    }

    const int textLeft = row.left() + kTextLeft;
    const int textWidth = row.right() - textLeft - 6;
    QFont nameFont = option.font;
    nameFont.setBold(true);
    QFont subFont = option.font;
    subFont.setPointSizeF(qMax(7.0, option.font.pointSizeF() - 1.0));

    QColor nameColor = pal.color(QPalette::Text);
    QColor subColor = pal.color(QPalette::PlaceholderText);
    if (!enabled) {
        nameColor.setAlpha(120);
        subColor.setAlpha(110);
    }

    const QFontMetrics nameMetrics(nameFont);
    const QFontMetrics subMetrics(subFont);
    const int blockHeight = nameMetrics.height() + 2 + subMetrics.height();
    const int top = row.center().y() - blockHeight / 2;

    painter->setFont(nameFont);
    painter->setPen(nameColor);
    painter->drawText(QRect(textLeft, top, textWidth, nameMetrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                      nameMetrics.elidedText(index.data(DictionaryProfileModel::NameRole).toString(), Qt::ElideRight, textWidth));

    painter->setFont(subFont);
    painter->setPen(subColor);
    painter->drawText(QRect(textLeft, top + nameMetrics.height() + 2, textWidth, subMetrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                      subMetrics.elidedText(index.data(DictionaryProfileModel::SubtitleRole).toString(), Qt::ElideRight, textWidth));
    painter->restore();
}

bool DictionaryProfileDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) {
    if (event->type() != QEvent::MouseButtonRelease) return false;
    const auto* mouse = static_cast<QMouseEvent*>(event);
    if (mouse->button() != Qt::LeftButton || !checkRect(option.rect).adjusted(-4, -4, 4, 4).contains(mouse->pos())) return false;
    model->setData(index, !index.data(DictionaryProfileModel::EnabledRole).toBool(), DictionaryProfileModel::EnabledRole);
    return true;
}
