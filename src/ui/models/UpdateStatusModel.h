#pragma once
#include <QString>
#include <QtGlobal>

// @brief 关于页里"检查更新"的状态机：该显示什么文字、哪些按钮可用、进度条多少。
//        只有纯逻辑，不含界面和网络代码，便于单元测试；界面和 UpdateManager 的信号都交给它转成状态。
//        没经过用户点击的结果（启动时的自动检查）一律忽略，所以关于页不会冒出"已是最新"。
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
    // 检查失败或下载失败
    void failed(const QString& message);

private:
    State m_state = State::Idle;
    QString m_version;   // 当前版本（UpToDate）或新版本（Available）
    QString m_error;
    int m_percent = 0;
    bool m_finished = false;
};
