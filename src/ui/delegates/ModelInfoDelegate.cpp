#include "ModelInfoDelegate.h"
#include "ui/models/ModelInfoModel.h"
#include "sherpa/GpuVariant.h"
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
// 小分辨率屏幕上窗口会被压窄：行宽低于阈值时收紧指标/特性列，把宽度让给名称列，避免模型名被截成省略号
constexpr int kCompactBelow = 560;
constexpr int kCompactMetricWidth = 80;
constexpr int kCompactFeatureWidth = 88;
constexpr int kBarCount = 5;
const QColor kSuccessColor(46, 160, 67);
const QColor kFailColor(220, 53, 69);
}

ModelInfoDelegate::Columns ModelInfoDelegate::columns(const QRect& row)
{
    Columns c;
    const QRect r = row.adjusted(kPad, 0, -kPad, 0);
    const bool compact = r.width() < kCompactBelow;
    const int featureW = compact ? kCompactFeatureWidth : kFeatureWidth;
    const int metricW = compact ? kCompactMetricWidth : kMetricWidth;
    c.radio = QRect(r.left(), r.top(), kRadioWidth, r.height());
    c.features = QRect(r.right() - featureW + 1, r.top(), featureW, r.height());
    c.speed = QRect(c.features.left() - metricW, r.top(), metricW, r.height());
    c.accuracy = QRect(c.speed.left() - metricW, r.top(), metricW, r.height());
    c.name = QRect(c.radio.right() + 1, r.top(), c.accuracy.left() - c.radio.right() - 9, r.height());
    return c;
}

QRect ModelInfoDelegate::actionRect(const QRect& row)
{
    const QRect f = columns(row).features;
    constexpr int kActionHeight = 24;
    return QRect(f.left(), f.center().y() - kActionHeight / 2, f.width(), kActionHeight);
}

ModelInfoDelegate::Action ModelInfoDelegate::actionAt(const QRect& row, const QPoint& pos,
                                                      ModelDownloadTracker::State state)
{
    if (!actionRect(row).contains(pos)) return Action::None;
    switch (state) {
    case ModelDownloadTracker::State::Downloading: return Action::Cancel;
    case ModelDownloadTracker::State::Failed:      return Action::Retry;
    case ModelDownloadTracker::State::Extracting:
    case ModelDownloadTracker::State::None:        break;
    }
    return Action::None;
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
    // 最后一行下方就是卡片边框，不再画分隔线
    if (index.row() < index.model()->rowCount() - 1) {
        QColor line = pal.color(QPalette::Text);
        line.setAlpha(28);
        p->setPen(line);
        p->drawLine(opt.rect.bottomLeft(), opt.rect.bottomRight());
    }

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

    // 下载状态：第二行在下载、解压、失败时换成对应的说明，没有任务时仍是覆盖语言
    const auto dlState = static_cast<ModelDownloadTracker::State>(index.data(ModelInfoModel::DownloadStateRole).toInt());
    const int dlPercent = index.data(ModelInfoModel::DownloadPercentRole).toInt();
    const QString dlText = index.data(ModelInfoModel::DownloadTextRole).toString();

    QFont subFont = opt.font;
    subFont.setPointSizeF(subFont.pointSizeF() * 0.9);
    p->setFont(subFont);
    QString subText = index.data(ModelInfoModel::LanguagesRole).toStringList().join(QStringLiteral(" · "));
    QColor subColor = pal.color(QPalette::PlaceholderText);
    switch (dlState) {
    case ModelDownloadTracker::State::Downloading:
        subText = dlText.isEmpty() ? tr("Starting download...") : dlText;
        subColor = pal.color(QPalette::Highlight);
        break;
    case ModelDownloadTracker::State::Extracting:
        subText = tr("Extracting... %1%").arg(dlPercent);
        subColor = pal.color(QPalette::Highlight);
        break;
    case ModelDownloadTracker::State::Failed:
        subText = tr("Download failed: %1").arg(dlText);
        subColor = kFailColor;
        break;
    case ModelDownloadTracker::State::None:
        break;
    }
    p->setPen(subColor);
    const QRect langRect(c.name.left(), nameRect.bottom() + 2, c.name.width(), 18);
    p->drawText(langRect, Qt::AlignLeft | Qt::AlignVCenter,
                QFontMetrics(subFont).elidedText(subText, Qt::ElideRight, langRect.width()));

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
    // 两项都显示 1.0~5.0 的评分（一位小数），具体错误率和耗时放在悬停提示里
    drawMetric(c.accuracy, index.data(ModelInfoModel::AccuracyLevelRole).toInt(),
               QString::number(index.data(ModelInfoModel::AccuracyRatingRole).toDouble(), 'f', 1));
    drawMetric(c.speed, index.data(ModelInfoModel::SpeedLevelRole).toInt(),
               QString::number(index.data(ModelInfoModel::SpeedRatingRole).toDouble(), 'f', 1));

    // 有下载任务时，特性那一列换成"取消 / 重试"，行内进度条画在行底边
    if (dlState != ModelDownloadTracker::State::None) {
        const QRect a = actionRect(opt.rect);
        p->setFont(subFont);
        if (dlState == ModelDownloadTracker::State::Downloading) {
            p->setPen(pal.color(QPalette::Highlight));
            p->drawText(a, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("✕ ") + tr("Cancel"));
        } else if (dlState == ModelDownloadTracker::State::Failed) {
            p->setPen(kFailColor);
            p->drawText(a, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("↻ ") + tr("Retry"));
        }
        constexpr int kBarHeight = 3;
        const QRect track(opt.rect.left(), opt.rect.bottom() - kBarHeight + 1, opt.rect.width(), kBarHeight);
        QColor trackColor = pal.color(QPalette::Mid);
        trackColor.setAlpha(60);
        p->setPen(Qt::NoPen);
        p->setBrush(trackColor);
        p->drawRect(track);
        if (dlState == ModelDownloadTracker::State::Failed) {
            p->setBrush(kFailColor);
            p->drawRect(track);
        } else {
            p->setBrush(pal.color(QPalette::Highlight));
            p->drawRect(QRect(track.left(), track.top(), track.width() * qBound(0, dlPercent, 100) / 100, kBarHeight));
        }
        p->restore();
        return;
    }

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
    // GPU 模式下的加速状态：不支持 / 没下载（点切换后自动下载）/ 已就绪
    const int gpuState = index.data(ModelInfoModel::GpuStateRole).toInt();
    if (gpuState == static_cast<int>(GpuVariant::GpuState::Ready)) {
        features << tr("GPU ready");
        dots << pal.color(QPalette::Highlight);
    } else if (gpuState == static_cast<int>(GpuVariant::GpuState::NotInstalled)) {
        features << tr("GPU not downloaded");
        dots << pal.color(QPalette::Mid);
    } else if (gpuState == static_cast<int>(GpuVariant::GpuState::Unsupported)) {
        features << tr("No GPU support");
        dots << pal.color(QPalette::Mid);
    }
    // 模型自己判断语种：短词、口音下可能串成别的语言，提示用户
    if (index.data(ModelInfoModel::AutoLanguageRole).toBool()) {
        features << tr("Auto language");
        dots << pal.color(QPalette::Mid);
    }
    p->setFont(subFont);
    // 项多时行高收紧，保证在 58px 的行内放得下
    const int lineH = features.size() > 3 ? 14 : (features.size() > 2 ? 16 : 18);
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
    const auto dlState = static_cast<ModelDownloadTracker::State>(index.data(ModelInfoModel::DownloadStateRole).toInt());
    if (dlState == ModelDownloadTracker::State::Failed) {
        // 失败原因在行里被省略号截断时，悬停看完整原因
        tip = tr("Download failed: %1").arg(index.data(ModelInfoModel::DownloadTextRole).toString());
    } else if (c.accuracy.contains(e->pos()) || c.speed.contains(e->pos())) {
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
    } else if (c.features.contains(e->pos())
               && index.data(ModelInfoModel::GpuStateRole).toInt() == static_cast<int>(GpuVariant::GpuState::NotInstalled)) {
        const double gb = index.data(ModelInfoModel::GpuSizeMbRole).toInt() / 1000.0;
        tip = tr("The GPU version (about %1 GB) is downloaded automatically when GPU mode is switched on").arg(gb, 0, 'f', 1);
    } else {
        tip = index.data(ModelInfoModel::RepoIdRole).toString();
    }
    QToolTip::showText(e->globalPos(), tip, view);
    return true;
}
