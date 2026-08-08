#ifndef WORKFLOWMANAGER_H
#define WORKFLOWMANAGER_H

#include <QObject>
#include "RecorderService.h"
#include "TranscriptionService.h"
#include "AppConfig.h"

enum class WorkflowState {
    Idle,           // 空闲，等待用户指令
    Loading,        // 正在加载模型或初始化
    Recording,      // 正在录音
    SavingAudio,    // 正在停止录音并保存文件
    Transcribing,   // 正在调用后端进行转录
    Processing,     // 正在进行文本后处理
    Stopping,       // 已请求停止，冲刷剩余待转录句
    Error           // 发生错误
};

enum class WorkflowEvent {
    StartRequested,
    ModelLoaded,
    ModelLoadFailed,
    UtteranceCaptured,
    UtteranceTranscribed,
    AllTranscribed,
    StopRequested,
    ErrorOccurred
};

class WorkflowManager : public QObject {
    Q_OBJECT
public:
    explicit WorkflowManager(const AppConfig &config, QObject *parent = nullptr);

    void initialize(AudioRecorderService *recorder,
                    TranscriptionService *transcription,
                    SherpaManager* sherpManager);

    // 用户意图级门面命令（替代 UI 直戳底层对象）
    void start();                              // → startRecording()
    void stop();                               // → stopRecording()
    void playTestTone();                       // → m_recorder->playTestTone()
    void applyRecorderConfig();                // → m_recorder->updateConfig()
    void preloadModel(const AppConfig& cfg);   // → m_sherpa->loadModelAsync(cfg, false)
    void togglePause();                        // → m_recorder->pause()/resume()

    WorkflowState state() const;               // 仅供 UI 显示，不用于分支逻辑

signals:
    void stateChanged(WorkflowState newState);
    void transcriptionResultReady(const QString &finalText);

private slots:
    void onUtteranceReady(const QByteArray& pcmData, int sampleRate);
    void onUtteranceTranscribed(bool success, const QString& rawText, const QString& finalText, const QString& errorMsg);
    void onInjectText(const QString& text);
    void onModelLoadFinished(bool ok);

private:
    // 内部实现（门面命令的具体执行）
    void startRecording();
    void stopRecording();

    // 集中式状态转移：校验合法性，非法转移打日志忽略
    void transitionTo(WorkflowState newState, WorkflowEvent evt);
    bool canTransition(WorkflowState from, WorkflowEvent evt, WorkflowState &out) const;

    void proceedToRecording();   // Loading 成功后真正开始监听

    QQueue<QString> m_injectQueue;
    bool m_injecting = false;
    void processInjectQueue();

    AudioRecorderService *m_recorder = nullptr;
    SherpaManager* m_sherpaManager = nullptr;
    TranscriptionService *m_transcription = nullptr;

    const AppConfig& m_config;
    WorkflowState m_currentState = WorkflowState::Idle;
    int m_pending = 0;          // 状态机内部待转录句计数（替代原 m_pendingTranscriptions）
};

#endif
