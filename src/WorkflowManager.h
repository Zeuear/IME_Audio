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
    ErrorOccurred,
    ErrorHandled
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
    int pendingCount() const { return m_pending; }              

signals:
    void stateChanged(WorkflowState newState);
    void transcriptionResultReady(const QString &finalText);
    // id 供实时字幕把正式结果对回原来那一句；对不上号的结果 id 为 0
    void utteranceAccepted(quint64 id);
    void utteranceFinished(const QString &finalText, quint64 id);
    void errorOccurred(const QString &title, const QString &cause = {});

private slots:
    void onUtteranceReady(const QByteArray& pcmData, int sampleRate);
    void onUtteranceTranscribed(bool success, const QString& rawText, const QString& finalText, const QString& errorMsg);
    void onInjectText(const QString& text);
    void onModelLoadFinished(bool ok);
    void onRecorderError(const QString& title, const QString& cause);

private:
    void startRecording();
    void stopRecording();

    void transitionTo(WorkflowState newState, WorkflowEvent evt);
    bool canTransition(WorkflowState from, WorkflowEvent evt, WorkflowState &out) const;
    void proceedToRecording();
    void abortRecording(WorkflowEvent evt);

    QQueue<QString> m_injectQueue;
    bool m_injecting = false;
    void processInjectQueue();

    IRecorder *m_recorder = nullptr;
    ISherpaModel* m_sherpaManager = nullptr;
    ITranscription *m_transcription = nullptr;

    const AppConfig& m_config;
    WorkflowState m_currentState = WorkflowState::Idle;
    int m_pending = 0;
    // 转录结果按提交顺序回来，出队即得到对应句子的 id
    QQueue<quint64> m_utteranceIds;
    quint64 m_nextUtteranceId = 0;
    bool m_stopRequested = false;

};

#endif
