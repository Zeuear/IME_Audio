#pragma once
#include "utils/Firewall.h"
#include <QFrame>

class QLabel;
class QPushButton;
class TaskQueueManager;

// @brief 设置页的防火墙卡片：显示并一键放行 / 取消放行；增删规则要管理员权限，走 ElevatedProcessTask。
class FirewallCard : public QFrame
{
    Q_OBJECT
public:
    explicit FirewallCard(QWidget* parent = nullptr);

signals:
    void errorOccurred(const QString& title, const QString& cause = {});

protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void setupUi();
    void retranslateUi();
    void refresh();
    void applyState(Firewall::State state);
    void onButtonClicked();
    void runNetsh(const QStringList& args);

    Firewall::State m_state = Firewall::State::Missing;
    bool m_busy = false;
    bool m_refreshed = false;
    TaskQueueManager* m_tasks = nullptr;

    QLabel* m_title = nullptr;
    QLabel* m_desc = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_button = nullptr;
};
