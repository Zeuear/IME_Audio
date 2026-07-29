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
    Error           // 发生错误
};

class WorkflowManager : public QObject {
    Q_OBJECT
public:
    explicit WorkflowManager(const AppConfig &config, QObject *parent = nullptr);

    void initialize(AudioRecorderService *recorder,
                    TranscriptionService *transcription,
                    SherpaManager* sherpManager);

    void startRecording();
    void stopRecording();
    WorkflowState currentState() const;

signals:
    void stateChanged(WorkflowState newState);
    void transcriptionResultReady(const QString &finalText);

private slots:
    void onVoiceStarted();
    void onUtteranceReady(const QByteArray& pcmData, int sampleRate);
    void onUtteranceTranscribed(bool success, const QString& rawText, const QString& finalText, const QString& errorMsg);
    void onInjectText(const QString& text);

private:
    QQueue<QString> m_injectQueue;
    bool m_injecting = false;
    void processInjectQueue();

    void transitionTo(WorkflowState newState);

    AudioRecorderService *m_recorder = nullptr;
    SherpaManager* m_sherpaManager = nullptr;
    TranscriptionService *m_transcription = nullptr;

    const AppConfig& m_config;
    WorkflowState m_currentState = WorkflowState::Idle;
    int m_pendingTranscriptions = 0;
};

#endif 