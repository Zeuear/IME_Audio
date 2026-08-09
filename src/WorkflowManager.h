#ifndef WORKFLOWMANAGER_H
#define WORKFLOWMANAGER_H

#include <QObject>
#include <QString>
#include <QQueue>
#include "AppConfig.h"
#include "interfaces/workflow_interfaces.h"

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

    void initialize(IRecorder *recorder,
                    ITranscription *transcription,
                    ISherpaModel* sherpManager);

    void start();                            
    void stop();                             
    void playTestTone();                      
    void applyRecorderConfig();               
    void preloadModel(const AppConfig& cfg);  
    void togglePause();                      
    WorkflowState state() const;
    int pendingCount() const { return m_pending; }   // 测试用：当前待转录句计数              

signals:
    void stateChanged(WorkflowState newState);
    void transcriptionResultReady(const QString &finalText);
    void errorOccurred(const QString &errorMsg);

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

    IRecorder *m_recorder = nullptr;
    ISherpaModel* m_sherpaManager = nullptr;
    ITranscription *m_transcription = nullptr;

    const AppConfig& m_config;
    WorkflowState m_currentState = WorkflowState::Idle;
    int m_pending = 0;          // 状态机内部待转录句计数（替代原 m_pendingTranscriptions）
    bool m_stopping = false;    // 持久“正在停止/冲刷”意图：跨 Stopping→Transcribing 跳转不丢失；进 Idle/Error 时清除
};

#endif
