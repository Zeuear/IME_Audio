#include "ModelListWidget.h"
#include "ModelInfoDelegate.h"
#include "ModelInfoModel.h"
#include <QEvent>
#include <QListView>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QVBoxLayout>

namespace {
constexpr int kCardRadius = 8;

QColor headerColor(const QPalette& pal)
{
    // 表头比行底色向文字色偏一点，深浅主题都只是轻微区分
    const QColor base = pal.color(QPalette::Base);
    const QColor text = pal.color(QPalette::Text);
    constexpr double k = 0.07;
    return QColor::fromRgbF(base.redF() + (text.redF() - base.redF()) * k,
                            base.greenF() + (text.greenF() - base.greenF()) * k,
                            base.blueF() + (text.blueF() - base.blueF()) * k);
}

QColor borderColor(const QPalette& pal)
{
    // 取文字色的淡色：QSS 主题下 Mid 的深浅不稳定，边框容易过重
    QColor c = pal.color(QPalette::Text);
    c.setAlpha(38);
    return c;
}

// 表头：列标题与行共用 ModelInfoDelegate::columns，保证对齐；作为卡片的第一行，上方两角随卡片圆角
class HeaderBar : public QWidget {
public:
    explicit HeaderBar(QWidget* parent) : QWidget(parent) { setFixedHeight(34); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath bg;
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, 0);
        bg.addRoundedRect(r.adjusted(0, 0, 0, kCardRadius), kCardRadius, kCardRadius);
        p.setClipRect(rect());
        p.fillPath(bg, headerColor(palette()));
        p.setPen(borderColor(palette()));
        p.drawLine(QPointF(0, height() - 0.5), QPointF(width(), height() - 0.5));

        QFont f = font();
        f.setPointSizeF(f.pointSizeF() * 0.82);
        f.setBold(true);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
        p.setFont(f);
        p.setPen(palette().color(QPalette::PlaceholderText));
        const auto c = ModelInfoDelegate::columns(rect());
        const int flags = Qt::AlignLeft | Qt::AlignVCenter;
        p.drawText(c.radio.united(c.name), flags, ModelListWidget::tr("MODEL"));
        p.drawText(c.accuracy, flags, ModelListWidget::tr("ACCURACY"));
        p.drawText(c.speed, flags, ModelListWidget::tr("SPEED"));
        p.drawText(c.features, flags, ModelListWidget::tr("FEATURES"));
    }
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

    m_header = new HeaderBar(this);
    layout->addWidget(m_header);
    layout->addWidget(m_list);

    connect(m_list, &QListView::clicked, this, [this](const QModelIndex& index) {
        emit modelActivated(index.data(Qt::DisplayRole).toString());
    });

    retranslateUi();
}

// 翻译器在 MainWin 构造之后才安装（readIni），构造时 tr 的文字必须在语言切换时重设
void ModelListWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        this->retranslateUi();
    }
    QWidget::changeEvent(event);
}

void ModelListWidget::retranslateUi()
{
    // 表头与行内的“已安装/自带标点”在绘制时 tr，重绘即可刷新
    m_header->update();
    m_list->viewport()->update();
}

void ModelListWidget::setLanguage(const QString& language)
{
    m_model->setLanguage(language);
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
    p.setBrush(palette().color(QPalette::Base));
    p.drawRoundedRect(card, kCardRadius, kCardRadius);
}

void ModelListWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // 列表是方角的，最后一行的选中底色会盖住卡片下方圆角：按卡片圆角裁掉列表底部两角
    QPainterPath path;
    const QRect r = m_list->rect();
    path.addRoundedRect(QRectF(r).adjusted(0, -kCardRadius, 0, 0), kCardRadius - 1, kCardRadius - 1);
    m_list->setMask(QRegion(path.toFillPolygon().toPolygon()));
}
