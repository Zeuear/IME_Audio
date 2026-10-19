#include "ModelListWidget.h"
#include "ModelInfoDelegate.h"
#include "ModelInfoModel.h"
#include <QEvent>
#include <QListView>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <functional>

namespace {
constexpr int kCardRadius = 8;

QColor headerColor(const QPalette& pal)
{
    // 表头比行底色向文字色偏一点，深浅主题都只是轻微区分
    const QColor base = pal.color(QPalette::Base);
    const QColor text = pal.color(QPalette::Text);
    constexpr double k = -0.07;
    return QColor::fromRgbF(base.redF() + (text.redF() - base.redF()) * k,
                            base.greenF() + (text.greenF() - base.greenF()) * k,
                            base.blueF() + (text.blueF() - base.blueF()) * k);
}

QColor borderColor(const QPalette& pal)
{
    // 取文字色的淡色：QSS 主题下 Mid 的深浅不稳定，边框容易过重
    QColor c = pal.color(QPalette::Text);
    c.setAlpha(0);
    return c;
}

// 表头：准确度 / 速度两列可点击排序，当前排序列的标题后画一个小三角
class HeaderBar : public QWidget {
public:
    using SortKey = ModelInfoModel::SortKey;

    explicit HeaderBar(QWidget* parent) : QWidget(parent)
    {
        setFixedHeight(34);
        setMouseTracking(true);
    }

    std::function<void(SortKey)> onSortClicked;

    void setSortIndicator(SortKey key, Qt::SortOrder order)
    {
        m_key = key;
        m_order = order;
        update();
    }

protected:
    SortKey keyAt(const QPoint& pos) const
    {
        const auto c = ModelInfoDelegate::columns(rect());
        if (c.accuracy.contains(pos)) return SortKey::Accuracy;
        if (c.speed.contains(pos)) return SortKey::Speed;
        return SortKey::Default;
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        setCursor(keyAt(e->pos()) == SortKey::Default ? Qt::ArrowCursor : Qt::PointingHandCursor);
    }

    void mouseReleaseEvent(QMouseEvent* e) override
    {
        const SortKey key = keyAt(e->pos());
        if (e->button() == Qt::LeftButton && key != SortKey::Default && onSortClicked) onSortClicked(key);
    }

    void drawIndicator(QPainter& p, const QRect& col, const QString& title, const QFontMetrics& fm) const
    {
        constexpr double kW = 8, kH = 5;
        const double x = col.left() + fm.horizontalAdvance(title) + 6;
        const double cy = col.center().y();
        QPolygonF tri;
        // 降序（最好在前）箭头朝下，升序朝上
        if (m_order == Qt::DescendingOrder)
            tri << QPointF(x, cy - kH / 2) << QPointF(x + kW, cy - kH / 2) << QPointF(x + kW / 2, cy + kH / 2);
        else
            tri << QPointF(x, cy + kH / 2) << QPointF(x + kW, cy + kH / 2) << QPointF(x + kW / 2, cy - kH / 2);
        p.setPen(Qt::NoPen);
        p.setBrush(palette().color(QPalette::Text));
        p.drawPolygon(tri);
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath bg;
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, 0);
        bg.addRoundedRect(r.adjusted(0, 0, 0, kCardRadius), kCardRadius, kCardRadius);
        p.setClipRect(rect());
        p.fillPath(bg, headerColor(palette()));

        QFont f = font();
        f.setPointSizeF(f.pointSizeF() * 0.82);
        f.setBold(true);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
        p.setFont(f);
        p.setPen(palette().color(QPalette::PlaceholderText));
        const auto c = ModelInfoDelegate::columns(rect());
        const int flags = Qt::AlignLeft | Qt::AlignVCenter;
        p.drawText(c.radio.united(c.name), flags, ModelListWidget::tr("MODEL"));
        const QString accuracyTitle = ModelListWidget::tr("ACCURACY");
        const QString speedTitle = ModelListWidget::tr("SPEED");
        // 当前排序列的标题用正文色，其余保持淡色
        p.setPen(m_key == SortKey::Accuracy ? palette().color(QPalette::Text) : palette().color(QPalette::PlaceholderText));
        p.drawText(c.accuracy, flags, accuracyTitle);
        p.setPen(m_key == SortKey::Speed ? palette().color(QPalette::Text) : palette().color(QPalette::PlaceholderText));
        p.drawText(c.speed, flags, speedTitle);
        p.setPen(palette().color(QPalette::PlaceholderText));
        p.drawText(c.features, flags, ModelListWidget::tr("FEATURES"));
        if (m_key == SortKey::Accuracy) drawIndicator(p, c.accuracy, accuracyTitle, QFontMetrics(f));
        if (m_key == SortKey::Speed) drawIndicator(p, c.speed, speedTitle, QFontMetrics(f));
    }

private:
    SortKey m_key = SortKey::Default;
    Qt::SortOrder m_order = Qt::DescendingOrder;
};
}

ModelListWidget::ModelListWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void ModelListWidget::setupUi()
{
    // 表头 + 列表画在同一张圆角卡片里；1px 边距留给卡片边框
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);

    m_model = new ModelInfoModel(this);
    m_list = new QListView(this);
    m_list->setModel(m_model);
    m_list->setItemDelegate(new ModelInfoDelegate(m_list));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->setMouseTracking(true);     // 悬停高亮
    m_list->setUniformItemSizes(true);
    // 行数不多，整表展开，由外层页面滚动，避免嵌套滚动条
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setFrameShape(QFrame::NoFrame);

    auto* header = new HeaderBar(this);
    header->onSortClicked = [this](ModelInfoModel::SortKey key) { onHeaderSortClicked(key); };
    m_header = header;
    layout->addWidget(m_header);
    layout->addWidget(m_list);

    connect(m_list, &QListView::clicked, this, [this](const QModelIndex& index) {
        emit modelActivated(index.data(Qt::DisplayRole).toString());
    });

    retranslateUi();
}

void ModelListWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        this->retranslateUi();
    }
    QWidget::changeEvent(event);
}

void ModelListWidget::retranslateUi()
{
    m_header->update();
    m_list->viewport()->update();
}

// 点同一列：最好在前 → 最好在后 → 恢复默认；换一列则从最好在前开始
void ModelListWidget::onHeaderSortClicked(ModelInfoModel::SortKey key)
{
    if (key != m_sortKey) {
        m_sortKey = key;
        m_sortOrder = Qt::DescendingOrder;
    } else if (m_sortOrder == Qt::DescendingOrder) {
        m_sortOrder = Qt::AscendingOrder;
    } else {
        m_sortKey = ModelInfoModel::SortKey::Default;
        m_sortOrder = Qt::DescendingOrder;
    }
    applySort();
}

// 排序会重置模型，选中项要跟着模型走而不是停在原行号
void ModelListWidget::applySort()
{
    const QString selected = currentModel();
    m_model->setSort(m_sortKey, m_sortOrder);
    setCurrentModel(selected);
    static_cast<HeaderBar*>(m_header)->setSortIndicator(m_sortKey, m_sortOrder);
}

void ModelListWidget::setLanguage(const QString& language)
{
    m_model->setLanguage(language);
    m_sortKey = ModelInfoModel::SortKey::Default;
    m_sortOrder = Qt::DescendingOrder;
    static_cast<HeaderBar*>(m_header)->setSortIndicator(m_sortKey, m_sortOrder);
    fitHeight();
    if (m_model->rowCount() > 0) m_list->setCurrentIndex(m_model->index(0));
}

bool ModelListWidget::setCurrentModel(const QString& displayName)
{
    const int row = m_model->rowOfDisplayName(displayName);
    if (row < 0) return false;
    m_list->setCurrentIndex(m_model->index(row));
    return true;
}

QString ModelListWidget::currentModel() const
{
    const QModelIndex index = m_list->currentIndex();
    return index.isValid() ? index.data(Qt::DisplayRole).toString() : QString();
}

void ModelListWidget::refreshInstalled()
{
    m_model->refreshInstalled();
}

void ModelListWidget::fitHeight()
{
    const int rowH = m_model->rowCount() > 0 ? m_list->sizeHintForRow(0) : 0;
    m_list->setFixedHeight(rowH * m_model->rowCount() + 2 * m_list->frameWidth());
}

void ModelListWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF card = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(borderColor(palette()));
    p.setBrush(headerColor(palette()));
    p.drawRoundedRect(card, kCardRadius, kCardRadius);
}

void ModelListWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    QPainterPath path;
    const QRect r = m_list->rect();
    path.addRoundedRect(QRectF(r).adjusted(0, -kCardRadius, 0, 0), kCardRadius - 1, kCardRadius - 1);
    m_list->setMask(QRegion(path.toFillPolygon().toPolygon()));
}
