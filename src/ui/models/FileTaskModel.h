#pragma once
#include <QAbstractListModel>
#include <QList>
#include <QString>
#include "ui/format/SubtitleFormat.h"

// 一个待转录文件及其进度/结果。链接任务先处于 Downloading，下载完成后 path 指向本地音频，之后与普通文件一样
struct FileTask {
    // Downloading 放在最后，保持其余状态的数值不变
    enum class State { Pending, Decoding, Transcribing, Done, Failed, NoSpeech, Downloading };

    QString path;
    QString url;          // 非空表示链接任务
    QString title;
    quint64 linkId = 0;   // 下载器里的任务 id，把下载结果对回这一行
    bool isLink() const { return !url.isEmpty(); }
    QString displayName() const;
    qint64 size = 0;
    State state = State::Pending;
    int segDone = 0;
    int segTotal = 0;
    QString text;
    QList<SubtitleCue> cues;   // 按 VAD 分段记录的带时间字幕，导出 SRT 用
    QString error;
};

// @brief
// 文件转录页的任务列表模型：只保存队列与状态，不含任何转录逻辑；界面只通过它读写状态。
class FileTaskModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role {
        StateRole = Qt::UserRole + 1,   // FileTask::State（int）
        ProgressRole,                   // 已完成片段数
        TotalRole,                      // 总片段数（未知时为 0）
        StatusTextRole,                 // 卡片第二行的状态文字
        TextRole,                       // 转录文本
    };

    explicit FileTaskModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    const FileTask& task(int row) const { return m_tasks.at(row); }
    int count(FileTask::State state) const;

    // 返回实际新增数量（已存在的路径会被忽略）
    int addPaths(const QStringList& paths);

    // 返回新行号；同一链接已在列表里返回 -1
    int addLink(const QString& url);
    int rowOfLink(quint64 linkId) const;
    void setLinkId(int row, quint64 linkId);
    void setDownloadPercent(int row, int percent);
    void setTitle(int row, const QString& title);
    void setDownloaded(int row, const QString& path, const QString& title);
    void restartDownload(int row);
    void removeRows(QList<int> rows);
    void clear();

    void reset(int row);
    void setState(int row, FileTask::State state);
    void setProgress(int row, int done, int total);
    void appendText(int row, const QString& text);
    void appendCue(int row, const SubtitleCue& cue);
    void setFailed(int row, const QString& error);

private:
    void touch(int row);

    QList<FileTask> m_tasks;
};
