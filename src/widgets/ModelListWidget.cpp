#include "ModelListWidget.h"
#include "ModelInfoDelegate.h"
#include "ModelInfoModel.h"
#include <QEvent>
#include <QLabel>
#include <QListView>
#include <QPainter>
#include <QVBoxLayout>

namespace {
// 表头：列标题与行共用 ModelInfoDelegate::columns，保证对齐
class HeaderBar : public QWidget {
public:
    explicit HeaderBar(QWidget* parent) : QWidget(parent) { setFixedHeight(26); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        QFont f = font();
        f.setPointSizeF(f.pointSizeF() * 0.85);
        f.setBold(true);
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
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

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

    m_note = new QLabel(this);
    m_note->setWordWrap(true);
    QPalette notePal = m_note->palette();
    notePal.setColor(QPalette::WindowText, notePal.color(QPalette::PlaceholderText));
    m_note->setPalette(notePal);

    m_header = new HeaderBar(this);
    layout->addWidget(m_header);
    layout->addWidget(m_list);
    layout->addWidget(m_note);

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
    m_note->setText(tr("Accuracy and speed are measured on the FLEURS test set "
                       "(120 sentences per language, CPU). Hover for details."));
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
