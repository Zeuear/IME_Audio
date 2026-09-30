#pragma once
#include <QAbstractListModel>
#include <QList>
#include <QString>

// 一个待转录文件及其进度/结果
struct FileTask {
    enum class State { Pending, Decoding, Transcribing, Done, Failed, NoSpeech };

    QString path;
    qint64 size = 0;
    State state = State::Pending;
    int segDone = 0;
    int segTotal = 0;
    QString text;
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
    void removeRows(QList<int> rows);
    void clear();

    void reset(int row);
    void setState(int row, FileTask::State state);
    void setProgress(int row, int done, int total);
    void appendText(int row, const QString& text);
    void setFailed(int row, const QString& error);

private:
    void touch(int row);

    QList<FileTask> m_tasks;
};
