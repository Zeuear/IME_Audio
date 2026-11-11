#include "FirewallCard.h"
#include "utils/Logger.h"
#include "utils/ProcessManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtConcurrent>

FirewallCard::FirewallCard(QWidget* parent) : QFrame(parent)
{
    setupUi();
    retranslateUi();
    if (!Firewall::isSupported()) setVisible(false);
}

void FirewallCard::setupUi()
{
    setProperty("card", true);
    m_title = new QLabel(this);
    QFont f = m_title->font();
    f.setBold(true);
    m_title->setFont(f);
    m_desc = new QLabel(this);
    m_desc->setWordWrap(true);
    m_desc->setForegroundRole(QPalette::PlaceholderText);
    m_status = new QLabel(this);
    m_button = new QPushButton(this);
    connect(m_button, &QPushButton::clicked, this, &FirewallCard::onButtonClicked);

    auto* row = new QHBoxLayout;
    row->addWidget(m_status, 1);
    row->addWidget(m_button);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(15, 12, 15, 12);
    lay->setSpacing(8);
    lay->addWidget(m_title);
    lay->addWidget(m_desc);
    lay->addLayout(row);
}

void FirewallCard::retranslateUi()
{
    m_title->setText(tr("Firewall"));
    m_desc->setText(tr("Block this app from accessing the network in Windows Firewall (outbound). Model downloads, cloud recognition, AI enhancement and update checks will stop working. Changing the rule needs administrator permission."));
    applyState(m_state);
}

void FirewallCard::applyState(Firewall::State state)
{
    m_state = state;
    switch (state) {
    case Firewall::State::Blocked:
        m_status->setText(tr("Status: blocked"));
        m_button->setText(tr("Unblock"));
        break;
    case Firewall::State::StalePath:
        m_status->setText(tr("Status: rule points to another location"));
        m_button->setText(tr("Update rule"));
        break;
    case Firewall::State::Missing:
        m_status->setText(tr("Status: not blocked"));
        m_button->setText(tr("Block"));
        break;
    }
    m_button->setEnabled(!m_busy);
}

void FirewallCard::showEvent(QShowEvent* event)
{
    QFrame::showEvent(event);
    // 首次显示才查询，netsh 要起子进程
    if (!m_refreshed && Firewall::isSupported()) refresh();
}

void FirewallCard::refresh()
{
    m_refreshed = true;
    m_busy = true;
    applyState(m_state);
    const QString exe = QCoreApplication::applicationFilePath();
    auto* watcher = new QFutureWatcher<Firewall::State>(this);
    connect(watcher, &QFutureWatcher<Firewall::State>::finished, this, [this, watcher]() {
        m_busy = false;
        applyState(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([exe]() { return Firewall::state(exe); }));
}

void FirewallCard::onButtonClicked()
{
    if (m_busy) return;
    const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    switch (m_state) {
    case Firewall::State::Blocked:   runNetsh(Firewall::removeArgs()); break;
    case Firewall::State::StalePath: runNetsh(Firewall::updateArgs(exe)); break;
    case Firewall::State::Missing:   runNetsh(Firewall::addArgs(exe)); break;
    }
}

void FirewallCard::runNetsh(const QStringList& args)
{
    LOG_INFO(QString("Firewall: running netsh %1").arg(args.join(' ')));
    if (!m_tasks) m_tasks = new TaskQueueManager(this);
    m_busy = true;
    applyState(m_state);

    auto* task = new ElevatedProcessTask(QStringLiteral("netsh.exe"), args);
    connect(task, &BaseTask::taskFinished, this, [this](TaskResult result, const QString& msg) {
        m_busy = false;
        if (result == TaskResult::Cancelled) {
            LOG_INFO("Firewall: user declined UAC");
            emit errorOccurred(QStringLiteral("已取消"), QStringLiteral("没有获得管理员权限，防火墙规则未改动"));
        } else if (result == TaskResult::Failed) {
            LOG_WARN(QString("Firewall: netsh failed: %1").arg(msg));
            emit errorOccurred(QStringLiteral("防火墙设置失败"), QStringLiteral("修改防火墙规则没有成功，详细信息见日志"));
        }
        refresh();
    });
    m_tasks->addTask(task);
}

void FirewallCard::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) retranslateUi();
    QFrame::changeEvent(event);
}
