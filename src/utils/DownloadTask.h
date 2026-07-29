#pragma once
#include <QString>
#include <QUrl>

enum class TaskStatus {
    Pending,     // 等待中
    Downloading, // 下载中
    Finished,    // 已完成
    Error,       // 失败
    Cancelled    // 已取消
};

using TaskFinishedCallback = std::function<void()>;
using TaskErrorCallback = std::function<void(const QString& err)>;

struct DownloadTask {
	QString groupId; // 可选的分组ID, 用于将多个下载任务归为一组
    QString id;             
    QUrl url;
    QString savePath;
    TaskStatus status = TaskStatus::Pending;
    QString errorMessage;

    qint64 received = 0;
    qint64 total = -1;
    double speed = 0.0;

    TaskFinishedCallback onComplete = nullptr;
    TaskErrorCallback onError = nullptr;

    DownloadTask(const QUrl& u, const QString& p) : url(u), savePath(p) {
        id = u.toString() + "|" + p; 
    }   
};