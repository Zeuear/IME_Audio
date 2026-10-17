#include "NavListWidget.h"
#include <QLabel>
#include <QHBoxLayout>
#include <QStyle>

namespace {
constexpr int kItemHeight = 22;
}

NavListItemWidget::NavListItemWidget(const QString& iconText, const QString& text, QWidget* parent)
    : QWidget(parent)
{
    // 紧凑的状态栏式条目：圆点 + 文字，圆点表示当前页，下载列表还用它显示下载状态
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(9, 0, 10, 0);
    layout->setSpacing(6);

    //if (iconText.isEmpty()) {
    //    m_iconLabel = new QLabel(iconText, this);
    //    m_iconLabel->setObjectName("navItemIcon");
    //    m_iconLabel->setFixedWidth(20);
    //    m_iconLabel->setAlignment(Qt::AlignCenter);
    //    layout->addWidget(m_iconLabel);
    //}

    m_indicatorDot = new QLabel(this);
    m_indicatorDot->setObjectName("navItemIndicator");
    m_indicatorDot->setFixedSize(6, 6);
    layout->addWidget(m_indicatorDot, 0, Qt::AlignVCenter);

    m_textLabel = new QLabel(text, this);
    m_textLabel->setObjectName("navItemText");
    layout->addWidget(m_textLabel, 1);
}

void NavListItemWidget::setActive(bool active)
{
    // 样式表按 active 属性切换圆点与文字颜色，属性变化后要重新 polish 才生效
    for (QLabel* label : {m_indicatorDot, m_textLabel}) {
        label->setProperty("active", active);
        label->style()->unpolish(label);
        label->style()->polish(label);
    }
}

int NavListItemWidget::preferredWidth() const
{
    // 子控件要单独 polish 才会带上样式表里的字号，否则按默认字体量出来偏窄，文字右侧会被截
    m_textLabel->ensurePolished();
    const QMargins m = layout()->contentsMargins();
    return m.left() + m_indicatorDot->width() + layout()->spacing()
         + m_textLabel->sizeHint().width() + m.right() + 4;
}

void NavListItemWidget::setIndicatorState(IndicatorState state)
{
    m_state = state;

    switch (state) {
    case IndicatorState::None:
        // 没有下载状态时圆点仍在，只表示是否当前页
        m_indicatorDot->setProperty("indicatorColor", QVariant());
        m_indicatorDot->style()->unpolish(m_indicatorDot);
        m_indicatorDot->style()->polish(m_indicatorDot);
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
    setSpacing(2);


    connect(this, &QListWidget::currentRowChanged, this, &NavListWidget::onCurrentRowChanged);
}

void NavListWidget::addNavItem(const QString& id, const QString& iconText, const QString& text)
{
    auto* item = new QListWidgetItem(this);
    addItem(item);

    auto* itemWidget = new NavListItemWidget(iconText, text, this);
    // 宽度跟文字走，不再固定 90px；样式表在构造后才应用，先 ensurePolished 让字号生效再量宽度
    itemWidget->ensurePolished();
    item->setSizeHint(QSize(itemWidget->preferredWidth(), kItemHeight));
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
    for (int i = 0; i < count(); ++i) {
        if (auto* w = qobject_cast<NavListItemWidget*>(itemWidget(this->item(i)))) w->setActive(i == row);
    }
    QListWidgetItem* item = this->item(row);
    if (!item) return;
    QString id = m_idsByItem.value(item);
    if (!id.isEmpty()) {
        emit navItemSelected(id);
    }
}