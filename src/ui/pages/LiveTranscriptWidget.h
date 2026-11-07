#pragma once
#include <QDateTime>
#include <QWidget>
#include <memory>

class QCheckBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTimer;
class TranscriptDocument;
class TranscriptStore;

// @brief "实时转录"页：把听写结果连续写成一整篇稿子（新闻现场、会议等长时间转录）。
//        不依赖任何服务：开始/停止、"同时输入到前台"通过信号交给宿主，识别结果由宿主调用 appendUtterance 送入。
//        稿子只在末尾追加，录音期间用户可以随意编辑前文。
class LiveTranscriptWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LiveTranscriptWidget(QWidget* parent = nullptr);
    ~LiveTranscriptWidget() override;

    // 追加一句定稿（空白句忽略）
    void appendUtterance(const QString& text, const QDateTime& time = QDateTime::currentDateTime());
    // 宿主告知录音是否进行中，用来切换"开始/停止"按钮
    void setRecording(bool recording);

    bool injectToForeground() const;
    void setInjectToForeground(bool on);

    // 指定自动保存的文件：立刻恢复上次的稿子，之后内容变化约 2 秒后合并写入，退出程序前再写一次。
    // 点"清空"会同时删除这个文件。不调用则不保存
    void setSessionFile(const QString& path);

signals:
    void startRequested();
    void stopRequested();
    // title/cause 为中文，由宿主统一弹出
    void errorOccurred(const QString& title, const QString& cause);
    // 勾选"同时输入到前台窗口"时为 true
    void injectToggled(bool on);

protected:
    void changeEvent(QEvent* event) override;

private:
    void setupUi();
    void retranslateUi();
    void updateButtons();
    void copyAll();
    void exportText();
    void scheduleSave();
    void flushSave();
    void clearAll();

    QPushButton* m_toggleBtn = nullptr;
    QCheckBox* m_injectCheck = nullptr;
    QCheckBox* m_timestampCheck = nullptr;
    QPushButton* m_copyBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QPlainTextEdit* m_edit = nullptr;
    std::unique_ptr<TranscriptDocument> m_transcript;
    std::unique_ptr<TranscriptStore> m_store;
    QTimer* m_saveTimer = nullptr;

    bool m_recording = false;
    bool m_dirty = false;       // 有还没写进文件的改动
    bool m_restoring = false;   // 正在恢复上次稿子，不触发保存
};
