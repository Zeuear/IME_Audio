#include "DownloadManager.h"
#include <QtConcurrent>

DownloadManager::DownloadManager(QNetworkAccessManager* nam, QObject* parent) : QObject(parent), m_nam(nam)
{
}

DownloadManager::~DownloadManager()
{
    cancelAll();
}

QString DownloadManager::addTask(const QUrl& url, const QString& savePath, TaskFinishedCallback onComplete, TaskErrorCallback onError)
{
    DownloadTask* task = new DownloadTask(url, savePath);
    task->onComplete = onComplete;
    task->onError = onError;
    m_tasks.append(task);
    m_queue.enqueue(task);

    emit taskAdded(task);
    emit queueChanged();

    // 先取出 id 再启动：同步失败时 startNext 会删掉 task，之后读 task->id 是悬垂访问（0xc0000374 崩溃）。
    const QString id = task->id;
    startNext();
    return id;
}

void DownloadManager::startNext()
{
    if (m_runningCount >= 1 || m_queue.isEmpty()) {
        return;
    }

    DownloadTask* task = m_queue.dequeue();
    task->status = TaskStatus::Downloading;
    m_activeMap.insert(task->id, task);
    m_runningCount++;

    m_currentDownloader = new Downloader(m_nam);
    connect(m_currentDownloader, &Downloader::progress, this, [this, task](const QString& fileName, qint64 rec, qint64 tot, double spd){
        task->received = rec;
        task->total = tot;
        task->speed = spd;
        emit taskProgress(task->id, rec, tot, spd);

        if (!task->groupId.isEmpty()) {
            updateGroupOnProgress(task, rec, tot);
        }
    });

    connect(m_currentDownloader, &Downloader::finished, this, &DownloadManager::onDownloaderFinished);
    connect(m_currentDownloader, &Downloader::error, this, &DownloadManager::onDownloaderError);

    m_currentDownloader->start(task->url, task->savePath);
}

void DownloadManager::onDownloaderFinished(const QString& savePath)
{

    DownloadTask* task = findActiveTaskBySavePath(savePath);
    if (task) {
        task->status = TaskStatus::Finished;
        emit taskFinished(task->id, savePath);
        m_activeMap.remove(task->id);

        if (!task->groupId.isEmpty()) {
            updateGroupOnFinished(task);
        }

        // 回调在当前线程同步执行，不丢线程池，避免回调里操作 UI 跨线程。
        if (task->onComplete) {
            task->onComplete();
        }
    }
    m_currentDownloader->deleteLater();
    m_currentDownloader = nullptr;
    m_runningCount--;

    startNext(); 
    emit queueChanged();

    if (task) {
        m_tasks.removeAll(task);
        delete task;
        task = nullptr;
    }
}

void DownloadManager::onDownloaderError(const QString& error)
{
    if (m_activeMap.isEmpty()) return;

    DownloadTask* task = m_activeMap.first();
    task->status = TaskStatus::Error;
    task->errorMessage = error;
    emit taskError(task->id, error);
    m_activeMap.remove(task->id);

    if (!task->groupId.isEmpty()) {
        updateGroupOnError(task, error);
    }

    if (task->onError) {
        task->onError(error);
    }
    if (m_currentDownloader) {
        m_currentDownloader->deleteLater();
        m_currentDownloader = nullptr;
    }
    m_runningCount--;

    startNext();
    emit queueChanged();

    if (task) {
        m_tasks.removeAll(task);
        delete task;
        task = nullptr;
    }
}

void DownloadManager::onDownloaderProgress(const QString& fileName, qint64 received, qint64 total, double speed)
{
}

void DownloadManager::cancelTask(const QString& taskId)
{
    for (int i = 0; i < m_queue.size(); ++i) {
        if (m_queue[i]->id == taskId) {
            DownloadTask* task = m_queue[i];
            m_queue.removeAt(i);
            task->status = TaskStatus::Cancelled;

            if (!task->groupId.isEmpty()) {
                auto it = m_groups.find(task->groupId);
                if (it != m_groups.end()) {
                    it.value().failedFiles += 1;
                    maybeEmitGroupFinished(task->groupId);
                }
            }

            emit queueChanged();
            return;
        }
    }

    if (m_activeMap.contains(taskId)) {
        DownloadTask* task = m_activeMap[taskId];
        if (m_currentDownloader) {
            m_currentDownloader->cancel();
        }
        task->status = TaskStatus::Cancelled;
    }
}


void DownloadManager::cancelGroup(const QString& groupId)
{
    // 排队中的要同时从任务表删除，否则 tasksInGroup 永远非空、isInstalling 恒真，取消后无法重下。
    for (int i = m_queue.size() - 1; i >= 0; --i) {
        DownloadTask* task = m_queue[i];
        if (task->groupId != groupId) continue;
        m_queue.removeAt(i);
        m_tasks.removeAll(task);
        delete task;
    }

    // 下载中的要归还 m_runningCount，否则之后所有下载被 startNext 挡住，队列卡死。
    bool releasedActive = false;
    for (auto it = m_activeMap.begin(); it != m_activeMap.end();) {
        DownloadTask* task = it.value();
        if (task->groupId != groupId) { ++it; continue; }
        it = m_activeMap.erase(it);
        if (m_currentDownloader) {
            m_currentDownloader->cancel();
            m_currentDownloader->deleteLater();
            m_currentDownloader = nullptr;
        }
        --m_runningCount;
        m_tasks.removeAll(task);
        delete task;
        releasedActive = true;
    }

    m_groups.remove(groupId);
    emit groupFinished(groupId, false);
    emit queueChanged();
    if (releasedActive) startNext();
}


void DownloadManager::cancelAll()
{
    m_queue.clear();
    if (m_currentDownloader) {
        m_currentDownloader->cancel();
    }
    for (auto task : m_tasks) {
        if (task->status == TaskStatus::Downloading) {
            task->status = TaskStatus::Cancelled;
        }
        delete task;
    }
    m_tasks.clear();
    m_activeMap.clear();
    m_runningCount = 0;
    emit queueChanged();
}

QString DownloadManager::addGroupTask(const QString& groupId, const QString& displayName,
                                      const QUrl& url, const QString& savePath,
                                      TaskFinishedCallback onComplete, TaskErrorCallback onError)
{
    DownloadTask* task = new DownloadTask(url, savePath);
    task->groupId = groupId;
    task->onComplete = onComplete;
    task->onError = onError;
    m_tasks.append(task);
    m_queue.enqueue(task);

    bool isNewGroup = !m_groups.contains(groupId);
    DownloadGroup& group = m_groups[groupId];
    if (isNewGroup) {
        group.groupId = groupId;
        group.displayName = displayName;
    }
    group.totalFiles += 1;

    emit taskAdded(task);
    emit queueChanged();
    emit groupStarted(groupId, group.displayName, group.totalFiles);

    const QString id = task->id;
    startNext();
    return id;
}

void DownloadManager::updateGroupOnProgress(DownloadTask* task, qint64 received, qint64 total)
{
    auto it = m_groups.find(task->groupId);
    if (it == m_groups.end()) return;
    DownloadGroup& group = it.value();

    group.fileReceived[task->id] = received;
    group.fileTotal[task->id] = total;

    qint64 sumReceived = 0, sumTotal = 0;
    for (auto rit = group.fileReceived.constBegin(); rit != group.fileReceived.constEnd(); ++rit) {
        sumReceived += rit.value();
    }
    for (auto tit = group.fileTotal.constBegin(); tit != group.fileTotal.constEnd(); ++tit) {
        sumTotal += tit.value();
    }

    int overallPercent = (sumTotal > 0) ? static_cast<int>(sumReceived * 100 / sumTotal) : 0;
    QString filename = QFileInfo(task->savePath).fileName();

    emit groupFileProgress(task->groupId, task->id, filename, received, total, overallPercent);
}

void DownloadManager::updateGroupOnFinished(DownloadTask* task)
{
    auto it = m_groups.find(task->groupId);
    if (it == m_groups.end()) return;
    DownloadGroup& group = it.value();

    group.finishedFiles += 1;
    QString filename = QFileInfo(task->savePath).fileName();
    emit groupFileFinished(task->groupId, task->id, filename);

    maybeEmitGroupFinished(task->groupId);
}

void DownloadManager::updateGroupOnError(DownloadTask* task, const QString& error)
{
    auto it = m_groups.find(task->groupId);
    if (it == m_groups.end()) return;
    DownloadGroup& group = it.value();

    group.failedFiles += 1;
    QString filename = QFileInfo(task->savePath).fileName();
    emit groupFileError(task->groupId, task->id, filename, error);

    maybeEmitGroupFinished(task->groupId);
}

void DownloadManager::maybeEmitGroupFinished(const QString& groupId)
{
    auto it = m_groups.find(groupId);
    if (it == m_groups.end()) return;
    DownloadGroup& group = it.value();

    int settled = group.finishedFiles + group.failedFiles;
    if (settled >= group.totalFiles) {
        bool success = (group.failedFiles == 0);
        emit groupFinished(groupId, success);
        m_groups.remove(groupId);
    }
}


QList<DownloadTask*> DownloadManager::tasksInGroup(const QString& groupId) const
{
    QList<DownloadTask*> result;
    for (auto* t : m_tasks) {
        if (t->groupId == groupId) result.append(t);
    }
    return result;
}


DownloadTask* DownloadManager::findActiveTaskBySavePath(const QString& savePath) const
{
    for (auto it = m_activeMap.constBegin(); it != m_activeMap.constEnd(); ++it) {
        if (it.value()->savePath == savePath) return it.value();
    }
    return nullptr;
}