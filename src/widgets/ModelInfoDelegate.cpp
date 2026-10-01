#include "ModelInfoDelegate.h"
#include "ModelInfoModel.h"
#include <QAbstractItemView>
#include <QHelpEvent>
#include <QPainter>
#include <QToolTip>

namespace {
constexpr int kRowHeight = 58;
constexpr int kPad = 12;
constexpr int kRadioWidth = 30;
constexpr int kMetricWidth = 104;
constexpr int kFeatureWidth = 96;
constexpr int kBarCount = 5;
const QColor kSuccessColor(46, 160, 67);
}

ModelInfoDelegate::Columns ModelInfoDelegate::columns(const QRect& row)
{
    Columns c;
    const QRect r = row.adjusted(kPad, 0, -kPad, 0);
    c.radio = QRect(r.left(), r.top(), kRadioWidth, r.height());
    c.features = QRect(r.right() - kFeatureWidth + 1, r.top(), kFeatureWidth, r.height());
    c.speed = QRect(c.features.left() - kMetricWidth, r.top(), kMetricWidth, r.height());
    c.accuracy = QRect(c.speed.left() - kMetricWidth, r.top(), kMetricWidth, r.height());
    c.name = QRect(c.radio.right() + 1, r.top(), c.accuracy.left() - c.radio.right() - 9, r.height());
    return c;
}

void ModelInfoDelegate::drawBars(QPainter* p, const QRect& area, int level, const QPalette& pal)
{
    constexpr int kBarWidth = 4;
    constexpr int kBarGap = 2;
    constexpr int kMaxHeight = 14;
    const int baseY = area.center().y() + kMaxHeight / 2;
    QColor off = pal.color(QPalette::Mid);
    off.setAlpha(140);
    for (int i = 0; i < kBarCount; ++i) {
        const int h = 4 + (kMaxHeight - 4) * i / (kBarCount - 1);
        const QRectF bar(area.left() + i * (kBarWidth + kBarGap), baseY - h, kBarWidth, h);
        p->setPen(Qt::NoPen);
        p->setBrush(i < level ? pal.color(QPalette::Text) : off);
        p->drawRoundedRect(bar, 1, 1);
    }
}

QSize ModelInfoDelegate::sizeHint(const QStyleOptionViewItem& opt, const QModelIndex&) const
{
    return QSize(opt.rect.width(), kRowHeight);
}

void ModelInfoDelegate::paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& index) const
{
    const QPalette& pal = opt.palette;
    const bool selected = opt.state & QStyle::State_Selected;
    const bool hovered = opt.state & QStyle::State_MouseOver;

    p->save();
    p->setRenderHint(QPainter::Antialiasing);

    // 底色：选中用高亮色淡化，悬停轻微提亮；行间分隔线
    if (selected || hovered) {
        QColor bg = pal.color(QPalette::Highlight);
        bg.setAlpha(selected ? 40 : 16);
        p->fillRect(opt.rect, bg);
    }
    QColor line = pal.color(QPalette::Mid);
    line.setAlpha(110);
    p->setPen(line);
    p->drawLine(opt.rect.bottomLeft(), opt.rect.bottomRight());

    const Columns c = columns(opt.rect);

    // 单选圈：当前选中的模型
    const QRectF ring(c.radio.left() + 1, c.radio.center().y() - 8, 16, 16);
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(selected ? pal.color(QPalette::Highlight) : pal.color(QPalette::Mid), 1.5));
    p->drawEllipse(ring);
    if (selected) {
        p->setPen(Qt::NoPen);
        p->setBrush(pal.color(QPalette::Highlight));
        p->drawEllipse(ring.adjusted(4, 4, -4, -4));
    }

    // 名称 + 覆盖语言
    QFont nameFont = opt.font;
    nameFont.setBold(true);
    p->setFont(nameFont);
    p->setPen(pal.color(QPalette::Text));
    const QRect nameRect(c.name.left(), c.name.top() + 9, c.name.width(), 20);
    p->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                QFontMetrics(nameFont).elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, nameRect.width()));

    QFont subFont = opt.font;
    subFont.setPointSizeF(subFont.pointSizeF() * 0.9);
    p->setFont(subFont);
    p->setPen(pal.color(QPalette::PlaceholderText));
    const QRect langRect(c.name.left(), nameRect.bottom() + 2, c.name.width(), 18);
    const QString langs = index.data(ModelInfoModel::LanguagesRole).toStringList().join(QStringLiteral(" · "));
    p->drawText(langRect, Qt::AlignLeft | Qt::AlignVCenter,
                QFontMetrics(subFont).elidedText(langs, Qt::ElideRight, langRect.width()));

    // 准确度 / 速度：信号格 + 数值
    auto drawMetric = [&](const QRect& col, int level, const QString& value) {
        p->setFont(opt.font);
        if (level <= 0) {
            p->setPen(pal.color(QPalette::PlaceholderText));
            p->drawText(col, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("—"));
            return;
        }
        drawBars(p, col, level, pal);
        p->setPen(pal.color(QPalette::Text));
        p->drawText(col.adjusted(kBarCount * 6 + 8, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, value);
    };
    const double rtf = index.data(ModelInfoModel::RtfRole).toDouble();
    drawMetric(c.accuracy, index.data(ModelInfoModel::AccuracyLevelRole).toInt(),
               QString::number(index.data(ModelInfoModel::ScoreRole).toDouble(), 'f', 1));
    // 速度用“比实时快多少倍”，比 RTF 更直观
    drawMetric(c.speed, index.data(ModelInfoModel::SpeedLevelRole).toInt(),
               rtf > 0 ? QStringLiteral("%1×").arg(qRound(1.0 / rtf)) : QString());

    // 特性：已安装（成功语义色）/ 自带标点
    QStringList features;
    QList<QColor> dots;
    if (index.data(ModelInfoModel::InstalledRole).toBool()) {
        features << tr("Installed");
        dots << kSuccessColor;
    }
    if (index.data(ModelInfoModel::BuiltinPunctRole).toBool()) {
        features << tr("Punctuation");
        dots << pal.color(QPalette::Highlight);
    }
    p->setFont(subFont);
    const int lineH = 18;
    int y = c.features.center().y() - features.size() * lineH / 2;
    for (int i = 0; i < features.size(); ++i, y += lineH) {
        p->setPen(Qt::NoPen);
        p->setBrush(dots[i]);
        p->drawEllipse(QRectF(c.features.left(), y + lineH / 2.0 - 3, 6, 6));
        p->setPen(pal.color(QPalette::Text));
        p->drawText(QRect(c.features.left() + 12, y, c.features.width() - 12, lineH),
                    Qt::AlignLeft | Qt::AlignVCenter, features[i]);
    }

    p->restore();
}

bool ModelInfoDelegate::helpEvent(QHelpEvent* e, QAbstractItemView* view, const QStyleOptionViewItem& opt,
                                  const QModelIndex& index)
{
    if (!e || !index.isValid()) return false;
    const Columns c = columns(opt.rect);
    QString tip;
    if (c.accuracy.contains(e->pos()) || c.speed.contains(e->pos())) {
        if (!index.data(ModelInfoModel::BenchmarkedRole).toBool()) {
            tip = tr("Not benchmarked for this language yet");
        } else {
            const double score = index.data(ModelInfoModel::ScoreRole).toDouble();
            const QString metric = index.data(ModelInfoModel::MetricRole).toString().toUpper();
            const double rtf = index.data(ModelInfoModel::RtfRole).toDouble();
            tip = tr("%1 error rate %2% · %3 s to transcribe 10 s of speech (CPU)")
                      .arg(metric)
                      .arg(QString::number(100 - score, 'f', 1))
                      .arg(QString::number(rtf * 10, 'f', 1));
        }
    } else {
        tip = index.data(ModelInfoModel::RepoIdRole).toString();
    }
    QToolTip::showText(e->globalPos(), tip, view);
    return true;
}
