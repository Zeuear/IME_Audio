#include "ModelDownloadTracker.h"

ModelDownloadTracker::Info ModelDownloadTracker::info(const QString& repoId) const
{
    const auto it = m_entries.constFind(repoId);
    return it == m_entries.constEnd() ? Info{} : it->info;
}

void ModelDownloadTracker::onGroupStarted(const QString& repoId, const QString&, int totalFiles)
{
    Entry e;   // 重新开始（含失败后重试）：旧的原因、取消标记、进度都不带过来
    e.info.state = State::Downloading;
    e.progress.start(totalFiles);
    m_entries.insert(repoId, e);
    emit changed(repoId);
}

void ModelDownloadTracker::onFileProgress(const QString& repoId, const QString& filename, qint64, qint64, int overallPercent)
{
    Entry& e = entry(repoId);
    if (e.info.state == State::Extracting) return;
    e.info.state = State::Downloading;   // 信号连上之前就开始的下载，来了进度也当作下载中
    e.progress.update(filename, overallPercent);
    e.info.percent = e.progress.percent();
    e.info.text = e.progress.line();
    emit changed(repoId);
}

void ModelDownloadTracker::onFileFinished(const QString& repoId, const QString&)
{
    entry(repoId).progress.fileFinished();   // 下一次进度到来时才体现在说明行上，这里不必通知
}

void ModelDownloadTracker::onFileError(const QString& repoId, const QString&, const QString& error)
{
    entry(repoId).lastError = error;
}

void ModelDownloadTracker::onExtractStarted(const QString& repoId, int)
{
    Entry& e = entry(repoId);
    e.info.state = State::Extracting;
    e.info.percent = 0;
    e.info.text.clear();
    emit changed(repoId);
}

void ModelDownloadTracker::onExtractProgress(const QString& repoId, int current, int total)
{
    Entry& e = entry(repoId);
    e.info.state = State::Extracting;
    e.info.percent = total > 0 ? qBound(0, current * 100 / total, 100) : 0;
    emit changed(repoId);
}

void ModelDownloadTracker::onGroupFinished(const QString& repoId, bool success, const QString& message)
{
    Entry& e = entry(repoId);
    if (success || e.cancelled) {
        m_entries.remove(repoId);   // 装好了，或用户取消了：都没有状态要显示
    } else {
        e.info.state = State::Failed;
        e.info.percent = 0;
        e.info.text = e.lastError.isEmpty() ? message : e.lastError;
    }
    emit changed(repoId);
}

void ModelDownloadTracker::markCancelled(const QString& repoId)
{
    if (m_entries.contains(repoId)) m_entries[repoId].cancelled = true;
}
