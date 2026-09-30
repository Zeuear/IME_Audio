#include "FileTaskDelegate.h"
#include "FileTaskModel.h"
#include <QPainter>
#include <QPainterPath>
#include <QDateTime>

namespace {
constexpr int kTileWidth = 176;
constexpr int kTileHeight = 116;
constexpr int kTileGap = 6;
constexpr int kCardRadius = 8;
const QColor kSuccessColor(46, 160, 67);
const QColor kFailedColor(218, 54, 51);
}

QSize FileTaskDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const
{
    return QSize(kTileWidth, kTileHeight);
}

void FileTaskDelegate::paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& index) const
{
    using State = FileTask::State;
    const auto state = static_cast<State>(index.data(FileTaskModel::StateRole).toInt());
    const bool selected = opt.state & QStyle::State_Selected;
    const QPalette& pal = opt.palette;

    p->save();
    p->setRenderHint(QPainter::Antialiasing);

    // 卡片底
    const QRect card = opt.rect.adjusted(kTileGap, kTileGap, -kTileGap, -kTileGap);
    QColor bg = pal.color(QPalette::Base);
    QColor border = pal.color(QPalette::Mid);
    if (selected) {
        border = pal.color(QPalette::Highlight);
        bg = border;
        bg.setAlpha(36);
    }
    p->setBrush(bg);
    p->setPen(QPen(border, selected ? 2 : 1));
    p->drawRoundedRect(QRectF(card).adjusted(0.5, 0.5, -0.5, -0.5), kCardRadius, kCardRadius);

    // 状态图标（右下，与状态文字同一行）
    QString glyph;
    QColor glyphColor = pal.color(QPalette::PlaceholderText);
    switch (state) {
    case State::Done:         glyph = QStringLiteral("✓"); glyphColor = kSuccessColor; break;
    case State::Failed:       glyph = QStringLiteral("✗"); glyphColor = kFailedColor;  break;
    case State::NoSpeech:     glyph = QStringLiteral("—"); break;
    case State::Pending:      glyph = QStringLiteral("○"); break;
    case State::Decoding:
    case State::Transcribing: break;
    }
    constexpr int kPad = 10;
    constexpr int kIconWidth = 40;
    const QRect iconRect(card.right() - kPad - kIconWidth, card.top() + 50, kIconWidth, 18);
    bool percent = false;
    if (state == State::Transcribing) {
        const int total = index.data(FileTaskModel::TotalRole).toInt();
        const int done = index.data(FileTaskModel::ProgressRole).toInt();
        glyph = QStringLiteral("%1%").arg(total > 0 ? done * 100 / total : 0);
        glyphColor = pal.color(QPalette::Highlight);
        percent = true;
    }
    if (!glyph.isEmpty()) {
        QFont f = opt.font;
        f.setPointSizeF(f.pointSizeF() * (percent ? 1.0 : 1.25));
        f.setBold(true);
        p->setFont(f);
        p->setPen(glyphColor);
        p->drawText(iconRect, Qt::AlignRight | Qt::AlignVCenter, glyph);
    }

    // 文件名（最多两行）+ 状态文字
    const int left = card.left() + kPad;
    const int fullWidth = card.width() - 2 * kPad;

    QFont nameFont = opt.font;
    nameFont.setBold(true);
    p->setFont(nameFont);
    p->setPen(pal.color(QPalette::Text));
    // 按两行宽度截断，再按任意字符换行，中文长名也能正常显示
    const QString name = QFontMetrics(nameFont).elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, fullWidth * 2 - 12);
    p->drawText(QRect(left, card.top() + 8, fullWidth, 40), Qt::AlignLeft | Qt::AlignTop | Qt::TextWrapAnywhere, name);

    QFont subFont = opt.font;
    subFont.setPointSizeF(subFont.pointSizeF() * 0.9);
    p->setFont(subFont);
    QColor subColor = pal.color(QPalette::PlaceholderText);
    if (state == State::Done) subColor = kSuccessColor;
    else if (state == State::Failed) subColor = kFailedColor;
    p->setPen(subColor);
    const int subWidth = fullWidth - (glyph.isEmpty() ? 0 : kIconWidth + 4);
    const QString sub = QFontMetrics(subFont).elidedText(index.data(FileTaskModel::StatusTextRole).toString(), Qt::ElideRight, subWidth);
    p->drawText(QRect(left, card.top() + 50, subWidth, 18), Qt::AlignLeft | Qt::AlignVCenter, sub);

    // 进度条（仅处理中显示）
    if (state == State::Decoding || state == State::Transcribing) {
        const QRectF track(left, card.bottom() - 14, fullWidth, 5);
        QPainterPath trackPath;
        trackPath.addRoundedRect(track, 2.5, 2.5);
        QColor trackColor = pal.color(QPalette::Mid);
        trackColor.setAlpha(120);
        p->fillPath(trackPath, trackColor);

        p->setClipPath(trackPath);
        QRectF fill = track;
        if (state == State::Transcribing) {
            const int total = index.data(FileTaskModel::TotalRole).toInt();
            const int done = index.data(FileTaskModel::ProgressRole).toInt();
            fill.setWidth(total > 0 ? track.width() * done / total : 0);
        } else {
            // 未知进度：一小段来回滑动，由外部定时器驱动重绘
            const double t = (QDateTime::currentMSecsSinceEpoch() % 1200) / 1200.0;
            fill.setWidth(track.width() * 0.3);
            fill.moveLeft(track.left() + track.width() * (-0.3 + 1.3 * t));
        }
        p->fillRect(fill, pal.color(QPalette::Highlight));
    }

    p->restore();
}
