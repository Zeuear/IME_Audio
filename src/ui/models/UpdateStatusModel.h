#pragma once
#include <QString>
#include <QtGlobal>

// @brief 关于页检查更新的状态机（文字、按钮可用性、进度条），纯逻辑；未经用户点击的自动检查结果一律忽略。
class UpdateStatusModel
{
public:
    enum class State { Idle, Checking, UpToDate, Available, Downloading, Failed };

    struct View {
        QString text;
        bool checkEnabled = true;
        bool installVisible = false;
        bool progressVisible = false;
        int percent = 0;   // 0..100；-1 表示总大小未知（不确定进度）
    };

    State state() const { return m_state; }
    View view() const;

    // 用户在关于页点了检查更新，或检查更新之后点了下载，期间页面自己负责显示结果
    bool isUserDriven() const { return m_state != State::Idle; }

    void startCheck();
    void upToDate(const QString& currentVersion);
    void available(const QString& version);
    void startDownload();
    void progress(qint64 received, qint64 total);
    void downloadFinished();
    void failed(const QString& message);

private:
    State m_state = State::Idle;
    QString m_version;   // 当前版本（UpToDate）或新版本（Available）
    QString m_error;
    int m_percent = 0;
    bool m_finished = false;
};
