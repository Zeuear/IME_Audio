#include "UpdateStatusModel.h"
#include <QCoreApplication>
#include <algorithm>

namespace {
QString tr(const char* text)
{
    return QCoreApplication::translate("UpdateStatusModel", text);
}
}

UpdateStatusModel::View UpdateStatusModel::view() const
{
    View v;
    switch (m_state) {
    case State::Idle:
        if (m_finished) v.text = tr("Update downloaded");
        break;
    case State::Checking:
        v.text = tr("Checking for updates...");
        v.checkEnabled = false;
        break;
    case State::UpToDate:
        v.text = tr("You are on the latest version (%1)").arg(m_version);
        break;
    case State::Available:
        v.text = tr("New version %1 is available").arg(m_version);
        v.installVisible = true;
        break;
    case State::Downloading:
        v.text = tr("Downloading update...");
        v.checkEnabled = false;
        v.progressVisible = true;
        v.percent = m_percent;
        break;
    case State::Failed:
        v.text = tr("Check failed: %1").arg(m_error);
        break;
    }
    return v;
}

void UpdateStatusModel::startCheck()
{
    m_state = State::Checking;
    m_finished = false;
}

void UpdateStatusModel::upToDate(const QString& currentVersion)
{
    if (m_state != State::Checking) return;   // 自动检查的结果，页面不显示
    m_state = State::UpToDate;
    m_version = currentVersion;
}

void UpdateStatusModel::available(const QString& version)
{
    if (m_state != State::Checking) return;
    m_state = State::Available;
    m_version = version;
}

void UpdateStatusModel::startDownload()
{
    m_state = State::Downloading;
    m_percent = 0;
}

void UpdateStatusModel::progress(qint64 received, qint64 total)
{
    if (m_state != State::Downloading) return;
    m_percent = total > 0 ? static_cast<int>(std::clamp<qint64>(received * 100 / total, 0, 100)) : -1;
}

void UpdateStatusModel::downloadFinished()
{
    if (m_state != State::Downloading) return;
    m_state = State::Idle;   // 之后是否重启由宿主弹的确认框决定，页面只保留一句结果
    m_finished = true;
}

void UpdateStatusModel::failed(const QString& message)
{
    if (m_state != State::Checking && m_state != State::Downloading) return;
    m_state = State::Failed;
    m_error = message;
}
