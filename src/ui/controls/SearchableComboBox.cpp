#include "SearchableComboBox.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>

SearchableComboBox::SearchableComboBox(QWidget* parent) : QComboBox(parent)
{
    setupUi();
}

void SearchableComboBox::setupUi()
{
    setEditable(true);
    setInsertPolicy(QComboBox::NoInsert);

    auto* completer = new QCompleter(model(), this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    setCompleter(completer);

    lineEdit()->setReadOnly(true);
    lineEdit()->installEventFilter(this);
    // 列表弹出后会抢走鼠标，双击的第二下落在列表窗口上，只能在列表窗口这里识别
    view()->window()->installEventFilter(this);

    connect(lineEdit(), &QLineEdit::editingFinished, this, &SearchableComboBox::leaveEditMode);
    // 选中过滤结果后结束输入；排队执行，等 QComboBox 先按选中项切换 currentIndex
    connect(completer, QOverload<const QString&>::of(&QCompleter::activated), this,
            &SearchableComboBox::leaveEditMode, Qt::QueuedConnection);
}

void SearchableComboBox::showPopup()
{
    m_popupClock.start();
    QComboBox::showPopup();
}

bool SearchableComboBox::isEditing() const
{
    return !lineEdit()->isReadOnly();
}

bool SearchableComboBox::eventFilter(QObject* watched, QEvent* event)
{
    // 输入模式下装在 qApp 上：点到不接收焦点的空白处时焦点不变、收不到失焦，只能这样识别
    if (event->type() == QEvent::MouseButtonPress && isEditing() && watched->isWidgetType()) {
        auto* target = static_cast<QWidget*>(watched);
        QWidget* filterPopup = completer() ? completer()->popup() : nullptr;
        const bool inFilterPopup = filterPopup && (target == filterPopup || filterPopup->isAncestorOf(target));
        if (target != this && !isAncestorOf(target) && !inFilterPopup) leaveEditMode();
    }

    if (watched == lineEdit()) {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
            if (lineEdit()->isReadOnly()) {
                m_openingPress = true;
                showPopup();
                return true;
            }
            [[fallthrough]];
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseButtonRelease:
            // 双击第二下已在列表窗口处理；列表收起后 Qt 会把按下重放给输入框，吞掉以免取消全选
            if (m_swallowEditClick) {
                if (event->type() == QEvent::MouseButtonRelease) {
                    m_swallowEditClick = false;
                    lineEdit()->selectAll();
                }
                return true;
            }
            break;
        default:
            break;
        }
    } else if (watched == view()->window()) {
        const bool justOpened = m_popupClock.isValid()
                                && m_popupClock.elapsed() <= QApplication::doubleClickInterval();
        if (event->type() == QEvent::MouseButtonPress) {
            m_openingPress = false;
            const QPoint global = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
            const QRect editRect(lineEdit()->mapToGlobal(QPoint(0, 0)), lineEdit()->size());
            if (justOpened && editRect.contains(global)) {
                hidePopup();
                enterEditMode();
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease && m_openingPress) {
            // 手动 showPopup() 时，同一次点击的松开会被 Qt 当成点在列表外而收起；按“是否为弹出那一下的松开”吞掉，不按时间
            m_openingPress = false;
            return true;
        }
    }
    return QComboBox::eventFilter(watched, event);
}

// 输入框的焦点代理是下拉框本身，失焦事件由 QComboBox 直接转发给输入框、不经过事件过滤器，只能在这里处理
void SearchableComboBox::focusOutEvent(QFocusEvent* event)
{
    QComboBox::focusOutEvent(event);
    if (event->reason() != Qt::PopupFocusReason) leaveEditMode();
}

void SearchableComboBox::keyPressEvent(QKeyEvent* event)
{
    if (isEditing() && event->key() == Qt::Key_Escape) {
        leaveEditMode();
        return;
    }
    QComboBox::keyPressEvent(event);
}

void SearchableComboBox::enterEditMode()
{
    m_swallowEditClick = true;
    lineEdit()->setReadOnly(false);
    lineEdit()->setFocus(Qt::MouseFocusReason);
    lineEdit()->selectAll();
    qApp->installEventFilter(this);
}

void SearchableComboBox::leaveEditMode()
{
    if (!isEditing()) return;
    qApp->removeEventFilter(this);
    m_swallowEditClick = false;
    if (completer() && completer()->popup()) completer()->popup()->hide();
    setEditText(itemText(currentIndex()));
    lineEdit()->setReadOnly(true);
}
