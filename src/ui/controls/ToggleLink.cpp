#include "ToggleLink.h"
#include <QAbstractButton>
#include <QCheckBox>
#include <QEvent>
#include <QObject>

namespace {
// 复选框没有"可用状态变化"的信号，用事件过滤器接 EnabledChange
class EnabledWatcher : public QObject
{
public:
    EnabledWatcher(QAbstractButton* button, QCheckBox* box)
        : QObject(button), m_button(button), m_box(box)
    {
        box->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == m_box && event->type() == QEvent::EnabledChange) {
            m_button->setEnabled(m_box->isEnabled());
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QAbstractButton* m_button;
    QCheckBox* m_box;
};
}

namespace ToggleLink {

void connect(QAbstractButton* button, QCheckBox* box)
{
    button->setCheckable(true);
    button->setChecked(box->isChecked());
    button->setEnabled(box->isEnabled());

    QObject::connect(button, &QAbstractButton::toggled, box, [box](bool on) {
        if (box->isChecked() != on) box->setChecked(on);
    });
    QObject::connect(box, &QCheckBox::toggled, button, [button](bool on) {
        if (button->isChecked() != on) button->setChecked(on);
    });
    new EnabledWatcher(button, box);
}

}
