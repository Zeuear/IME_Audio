#include "NavListWidget.h"
#include <QLabel>
#include <QHBoxLayout>

NavListItemWidget::NavListItemWidget(const QString& iconText, const QString& text, QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(10);

    //if (iconText.isEmpty()) {
    //    m_iconLabel = new QLabel(iconText, this);
    //    m_iconLabel->setObjectName("navItemIcon");
    //    m_iconLabel->setFixedWidth(20);
    //    m_iconLabel->setAlignment(Qt::AlignCenter);
    //    layout->addWidget(m_iconLabel);
    //}

    m_textLabel = new QLabel(text, this);
    m_textLabel->setObjectName("navItemText");
    layout->addWidget(m_textLabel, 1);

    //m_indicatorDot = new QLabel(this);
    //m_indicatorDot->setObjectName("navItemIndicator");
    //m_indicatorDot->setFixedSize(8, 8);
    //m_indicatorDot->hide();
    //layout->addWidget(m_indicatorDot, 0, Qt::AlignVCenter);
}

void NavListItemWidget::setIndicatorState(IndicatorState state)
{
    m_state = state;

    switch (state) {
    case IndicatorState::None:
        m_indicatorDot->hide();
        break;
    case IndicatorState::Downloading:
        m_indicatorDot->setProperty("indicatorColor", "yellow");
        m_indicatorDot->style()->unpolish(m_indicatorDot);
        m_indicatorDot->style()->polish(m_indicatorDot);
        m_indicatorDot->show();
        break;
    case IndicatorState::Completed:
        m_indicatorDot->setProperty("indicatorColor", "green");
        m_indicatorDot->style()->unpolish(m_indicatorDot);
        m_indicatorDot->style()->polish(m_indicatorDot);
        m_indicatorDot->show();
        break;
    }
}


NavListWidget::NavListWidget(QWidget* parent)
    : QListWidget(parent)
{
    setObjectName("navListWidget");
    setSelectionMode(QAbstractItemView::SingleSelection);
    setFocusPolicy(Qt::NoFocus);
    setFrameShape(QFrame::NoFrame);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setViewMode(QListView::IconMode); 
    setFlow(QListView::LeftToRight);
    setResizeMode(QListView::Adjust);
    setMovement(QListWidget::Static);


    connect(this, &QListWidget::currentRowChanged, this, &NavListWidget::onCurrentRowChanged);
}

void NavListWidget::addNavItem(const QString& id, const QString& iconText, const QString& text)
{
    auto* item = new QListWidgetItem(this);
    item->setSizeHint(QSize(90, 32));

    addItem(item);

    auto* itemWidget = new NavListItemWidget(iconText, text, this);
    setItemWidget(item, itemWidget);

    m_itemsById.insert(id, item);
    m_idsByItem.insert(item, id);
}

void NavListWidget::setIndicator(const QString& id, NavListItemWidget::IndicatorState state)
{
    auto it = m_itemsById.find(id);
    if (it == m_itemsById.end()) return;

    QListWidgetItem* item = it.value();
    if (auto* w = qobject_cast<NavListItemWidget*>(itemWidget(item))) {
        w->setIndicatorState(state);
    }
}

void NavListWidget::selectItem(const QString& id)
{
    auto it = m_itemsById.find(id);
    if (it == m_itemsById.end()) return;
    setCurrentItem(it.value());
}

QString NavListWidget::currentItemId() const
{
    QListWidgetItem* item = currentItem();
    if (!item) return QString();
    return m_idsByItem.value(item);
}

void NavListWidget::onCurrentRowChanged(int row)
{
    QListWidgetItem* item = this->item(row);
    if (!item) return;
    QString id = m_idsByItem.value(item);
    if (!id.isEmpty()) {
        emit navItemSelected(id);
    }
}