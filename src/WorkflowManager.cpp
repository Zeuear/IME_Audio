#include "WorkflowManager.h"
#include "utils/Logger.h"
#include "InputInjector.h"
#include <QMessageBox>

WorkflowManager::WorkflowManager(const AppConfig &config, QObject *parent) : QObject(parent), m_config(config) {}

void WorkflowManager::initialize(AudioRecorderService *recorder,
                                TranscriptionService *transcription,
                                SherpaManager* sherpManager) 
{
    m_recorder = recorder;
    m_transcription = transcription;
    m_sherpaManager = sherpManager;

    // 语音检测:仅用于日志/UI提示,不驱动状态机
    connect(m_recorder, &AudioRecorderService::voiceStarted, this, &WorkflowManager::onVoiceStarted);

    // 一句话缓冲完成,直接交给识别服务,不经过文件
    connect(m_recorder, &AudioRecorderService::utteranceReady, this, &WorkflowManager::onUtteranceReady);

    // 识别结果回调
    connect(m_transcription, &TranscriptionService::transcriptionFinished, this, &WorkflowManager::onUtteranceTranscribed);

    connect(this, &WorkflowManager::transcriptionResultReady, this, &WorkflowManager::onInjectText);
}

void WorkflowManager::startRecording() {
    if (m_currentState != WorkflowState::Idle) return;
    LOG_DEBUG("Start Recording");
    transitionTo(WorkflowState::Loading);
    m_sherpaManager->pauseIdleTimer();

    if (m_config.backend == AsrBackendKind::Sherpa) {

        m_sherpaManager->loadModel(m_config);
        if (!m_sherpaManager->isModelLoaded()) {
            transitionTo(WorkflowState::Error);
            LOG_ERROR("Model failed to load");
            return;
        }
    }

    transitionTo(WorkflowState::Recording);
    if (!m_recorder->startListening()) {
        //if (m_config.backend == AsrBackendKind::Sherpa) {
        //    m_sherpaManager->unloadModel(); 
        //}
        transitionTo(WorkflowState::Error);
        return;
    }
}

void WorkflowManager::stopRecording() {
    if (m_currentState != WorkflowState::Recording) return;
    LOG_DEBUG("Stop Recording");

    m_recorder->stopListening();
    m_pendingTranscriptions = 0;
    transitionTo(WorkflowState::Idle);
    //if (m_config.backend == AsrBackendKind::Sherpa) {
    //    m_sherpaManager->unloadModel();
    //}
    // 结束监听：恢复空闲计时
    m_sherpaManager->resumeIdleTimer();
}

WorkflowState WorkflowManager::currentState() const
{
    return m_currentState;
}

void WorkflowManager::onVoiceStarted() {
    LOG_DEBUG("Voice detected...");
}

void WorkflowManager::onUtteranceReady(const QByteArray& pcmData, int sampleRate) {
    LOG_DEBUG(tr("Utterance captured, size: %1 bytes").arg(pcmData.size()));
    m_pendingTranscriptions++;
    transitionTo(WorkflowState::Transcribing);
    m_transcription->transcribe(pcmData, sampleRate, m_config.audio.channels, m_config.audio.bitsPerSample);
}

void WorkflowManager::onUtteranceTranscribed(bool success, const QString& rawText, const QString& finalText, const QString& errorMsg) {
    if (m_pendingTranscriptions > 0) {
        m_pendingTranscriptions--;
    }

    if (success) {
        if (!finalText.isEmpty()) {
            LOG_DEBUG(QString("Transcription success: %1").arg(finalText));
            emit transcriptionResultReady(finalText);
            transitionTo(WorkflowState::Processing);
        }
    }
    else {
        QMessageBox::warning(nullptr, tr("Error"), errorMsg);
        LOG_WARN(QString("Transcription failed: %1").arg(errorMsg));
        LOG_WARN(errorMsg);
        transitionTo(WorkflowState::Processing);
    }

    if (m_pendingTranscriptions == 0) {
        transitionTo(m_recorder->isListening() ? WorkflowState::Recording : WorkflowState::Idle);
    }
}

void WorkflowManager::transitionTo(WorkflowState newState) {
    if (m_currentState == newState) return;
    m_currentState = newState;
    emit stateChanged(m_currentState);
    //qDebug() << "Workflow State:" << static_cast<int>(m_currentState);
}

void WorkflowManager::onInjectText(const QString& text) {
    if (text.isEmpty()) return;
    m_injectQueue.enqueue(text);
    processInjectQueue();
}

void WorkflowManager::processInjectQueue() {
    if (m_injecting || m_injectQueue.isEmpty()) return;

    m_injecting = true;
    QString text = m_injectQueue.dequeue();

    bool ok = InputInjector::inject(text);
    if (!ok) {
        LOG_WARN(QString("InputInjector failed for text: %1").arg(text));
    }

    m_injecting = false;
    // 递归处理队列里剩余的内容(注入之间保留处理顺序)
    if (!m_injectQueue.isEmpty()) {
        QMetaObject::invokeMethod(this, &WorkflowManager::processInjectQueue, Qt::QueuedConnection);
    }
}