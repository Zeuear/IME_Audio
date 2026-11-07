#pragma once
#include <QDateTime>
#include <QWidget>
#include <memory>

class QCheckBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class TranscriptDocument;

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

signals:
    void startRequested();
    void stopRequested();
    // 勾选"同时输入到前台窗口"时为 true
    void injectToggled(bool on);

protected:
    void changeEvent(QEvent* event) override;

private:
    void setupUi();
    void retranslateUi();
    void updateButtons();
    void copyAll();

    QPushButton* m_toggleBtn = nullptr;
    QCheckBox* m_injectCheck = nullptr;
    QCheckBox* m_timestampCheck = nullptr;
    QPushButton* m_copyBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QPlainTextEdit* m_edit = nullptr;
    std::unique_ptr<TranscriptDocument> m_transcript;

    bool m_recording = false;
};
