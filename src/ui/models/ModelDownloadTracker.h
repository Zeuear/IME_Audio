#pragma once
#include <QHash>
#include <QObject>
#include <QString>
#include "utils/DownloadText.h"

// @brief 按模型记录下载状态（下载中、解压中、失败）；状态放这里而非列表模型，因切语言会重建列表而下载仍在继续。
class ModelDownloadTracker : public QObject
{
    Q_OBJECT
public:
    enum class State { None, Downloading, Extracting, Failed };

    struct Info {
        State state = State::None;
        int percent = 0;   // 下载中是整体百分比，解压中是解压百分比
        QString text;      // 下载中："文件名 · 第几个/共几个 · 百分比"；失败：原因
    };

    explicit ModelDownloadTracker(QObject* parent = nullptr) : QObject(parent) {}

    Info info(const QString& repoId) const;

    void onGroupStarted(const QString& repoId, const QString& displayName, int totalFiles);
    void onFileProgress(const QString& repoId, const QString& filename, qint64 received, qint64 total, int overallPercent);
    void onFileFinished(const QString& repoId, const QString& filename);
    void onFileError(const QString& repoId, const QString& filename, const QString& error);
    void onExtractStarted(const QString& repoId, int totalLines);
    void onExtractProgress(const QString& repoId, int current, int total);
    void onGroupFinished(const QString& repoId, bool success, const QString& message);

    // 用户主动取消：下载层随后会报告"组失败"，那不是真的失败，要当作没有状态
    void markCancelled(const QString& repoId);

signals:
    void changed(const QString& repoId);

private:
    struct Entry {
        Info info;
        DownloadGroupProgress progress;
        QString lastError;
        bool cancelled = false;
    };
    Entry& entry(const QString& repoId) { return m_entries[repoId]; }

    QHash<QString, Entry> m_entries;
};
