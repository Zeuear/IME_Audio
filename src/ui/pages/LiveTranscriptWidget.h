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

// @brief 实时转录页：听写结果连续写成整篇稿子；不依赖服务，开始/停止与注入开关经信号交宿主，结果由宿主调 appendUtterance。
class LiveTranscriptWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LiveTranscriptWidget(QWidget* parent = nullptr);
    ~LiveTranscriptWidget() override;

    void appendUtterance(const QString& text, const QDateTime& time = QDateTime::currentDateTime());
    // 宿主告知录音是否进行中，用来切换"开始/停止"按钮
    void setRecording(bool recording);

    bool injectToForeground() const;
    void setInjectToForeground(bool on);
    // 横条上的"同时输入到前台"开关用 ToggleLink 连到这个复选框，两处是同一个状态
    QCheckBox* injectCheckBox() const { return m_injectCheck; }

    // 指定自动保存文件：立刻恢复上次稿子，内容变化约 2 秒后合并写入，退出前再写一次，清空时删除；不调用则不保存
    void setSessionFile(const QString& path);

signals:
    void startRequested();
    void stopRequested();
    // title/cause 为中文，由宿主统一弹出
    void errorOccurred(const QString& title, const QString& cause);
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
