#pragma once
#include <QAbstractListModel>
#include <QList>
#include <QString>
#include "ui/format/SubtitleFormat.h"

// 一个待转录文件及其进度/结果
struct FileTask {
    // Downloading 放在最后，保持其余状态的数值不变
    enum class State { Pending, Decoding, Transcribing, Done, Failed, NoSpeech, Downloading, DownloadingModel, Separating };

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
    QList<SubtitleCue> cues;    // 按 VAD 分段记录的带时间字幕，导出 SRT 用
    QString error;
    QString vocalsPath;         // 分离出的人声 WAV（临时文件）；没开分离或还没分离完时为空

    QStringList translations;
    QString translationLang;    // 译文语言代码，导出文件名用
    quint64 runToken = 0;       // 每次重新转录换新值，上一轮迟到的译文据此丢弃

    // 耗时：startMs 是本轮转录开始的时间戳（0 表示没在跑）
    qint64 startMs = 0;
    qint64 transcribeStartMs = 0;
    qint64 elapsedMs = 0;

    bool hasTranslation() const;
    QList<SubtitleCue> translatedCues() const;
    QString translatedText() const;
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
        TimingTextRole,                 // 卡片第三行：进行中是"已用 / 预计剩余"，结束后是"耗时"
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
    // 缓存目录被清空后调用：人声路径一律失效；链接任务下载的音频已不在，path 置空，
    // 之后再转录会走"重新下载"。已有的文字和译文保留。返回受影响的行数
    int dropCachedFiles(const QString& linkDownloadsDir);

    void reset(int row);
    void markStarted(int row);   // 这一行开始转录，开始计时
    // 时长格式化 m:ss / h:mm:ss；按已完成段数推算剩余毫秒，推不出来（还没有完成的段）返回 -1
    static QString formatDuration(qint64 ms);
    static qint64 estimateRemainingMs(qint64 transcribeElapsedMs, int done, int total);
    void setState(int row, FileTask::State state);
    void setProgress(int row, int done, int total);
    // 分离人声阶段（下载模型 / 分离）的进度
    void setStageProgress(int row, FileTask::State state, int done, int total);
    void setVocalsPath(int row, const QString& path);
    void appendText(int row, const QString& text);
    void appendCue(int row, const SubtitleCue& cue);
    void setFailed(int row, const QString& error);
    // 按 runToken 找行：译文是异步回来的，行号可能已变、任务可能已删或已重转
    void setCueTranslation(quint64 runToken, int cue, const QString& lang, const QString& text);

private:
    static QString timingText(const FileTask& task);
    void touch(int row);

    QList<FileTask> m_tasks;
    quint64 m_nextToken = 0;
};
