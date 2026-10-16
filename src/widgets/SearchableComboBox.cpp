#include "SearchableComboBox.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
#include <QFocusEvent>
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

    // 输入了列表里没有的内容就离开时，文字回退到当前选中项，避免界面显示与实际选择不一致
    connect(lineEdit(), &QLineEdit::editingFinished, this, &SearchableComboBox::leaveEditMode);
}

void SearchableComboBox::showPopup()
{
    m_popupClock.start();
    QComboBox::showPopup();
}

bool SearchableComboBox::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == lineEdit()) {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
            if (lineEdit()->isReadOnly()) {
                showPopup();
                return true;
            }
            break;
        case QEvent::FocusOut:
            // 过滤结果弹窗出现时输入框也会失焦，这种不算离开
            if (static_cast<QFocusEvent*>(event)->reason() != Qt::PopupFocusReason) leaveEditMode();
            break;
        default:
            break;
        }
    } else if (watched == view()->window() && event->type() == QEvent::MouseButtonPress) {
        // 刚弹出列表就又点在输入框上 = 双击：收起列表，进入输入过滤
        const QPoint global = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
        const QRect editRect(lineEdit()->mapToGlobal(QPoint(0, 0)), lineEdit()->size());
        if (m_popupClock.isValid() && m_popupClock.elapsed() <= QApplication::doubleClickInterval()
            && editRect.contains(global)) {
            hidePopup();
            enterEditMode();
            return true;
        }
    }
    return QComboBox::eventFilter(watched, event);
}

void SearchableComboBox::enterEditMode()
{
    lineEdit()->setReadOnly(false);
    lineEdit()->setFocus(Qt::MouseFocusReason);
    lineEdit()->selectAll();
}

void SearchableComboBox::leaveEditMode()
{
    if (lineEdit()->isReadOnly()) return;
    setEditText(itemText(currentIndex()));
    lineEdit()->setReadOnly(true);
}
