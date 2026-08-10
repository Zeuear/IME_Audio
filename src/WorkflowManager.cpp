#include "WorkflowManager.h"
#include "utils/Logger.h"
#include "InputInjector.h"
#include <QMetaObject>

/*
* 运行流程
1. 加载模型   --> 显示加载页面，如果加载失败，则关闭页面
2. 开始录音   --> 显示出录音声谱界面，如果没有声音一段时间关闭窗口，如果有声音则显示窗口, 如果有错误则关闭窗口，提示错误
3. 转录音频   --> 显示出转录页面，转录结束后，如果关闭continue模式，则证明是按下停止之后转录的，这时关闭转录窗口，如果是开启
                  continue模式，则会触发Process模型，如果没有声音则会自动停止，同时也会触发Recording。因为接下来还会继续录音
4. 打印转录   --> 这个不会受影响。
*/

WorkflowManager::WorkflowManager(const AppConfig &config, QObject *parent) : QObject(parent), m_config(config) {}

void WorkflowManager::initialize(IRecorder *recorder,
                                ITranscription *transcription,
                                ISherpaModel* sherpManager)
{
    m_recorder = recorder;
    m_transcription = transcription;
    m_sherpaManager = sherpManager;

    // 模型加载完成
    connect(m_sherpaManager, &ISherpaModel::modelLoadFinished, this, &WorkflowManager::onModelLoadFinished);
    // 录音层错误（VAD 缺失等）透传
    connect(m_recorder, &IRecorder::errorOccurred, this, &WorkflowManager::onRecorderError);
    // 一句话缓冲完成，交给识别服务
    connect(m_recorder, &IRecorder::utteranceReady, this, &WorkflowManager::onUtteranceReady);
    connect(m_transcription, &ITranscription::transcriptionFinished, this, &WorkflowManager::onUtteranceTranscribed);
    // 识别结果打印
    connect(this, &WorkflowManager::transcriptionResultReady, this, &WorkflowManager::onInjectText);
}

void WorkflowManager::start() { startRecording(); }
void WorkflowManager::stop() { stopRecording(); }

void WorkflowManager::playTestTone()
{
    if (m_recorder) m_recorder->playTestTone();
}

void WorkflowManager::applyRecorderConfig()
{
    if (m_recorder) m_recorder->updateConfig();
}

void WorkflowManager::preloadModel(const AppConfig& cfg)
{
    if (m_sherpaManager) m_sherpaManager->loadModelAsync(cfg, false);
}

void WorkflowManager::togglePause()
{
    if (!m_recorder) return;
    if (m_recorder->isPaused()) m_recorder->resume();
    else m_recorder->pause();
}

WorkflowState WorkflowManager::state() const
{
    return m_currentState;
}

void WorkflowManager::startRecording() {
    if (m_currentState != WorkflowState::Idle) return;
    LOG_DEBUG("Start Recording");
    transitionTo(WorkflowState::Loading, WorkflowEvent::StartRequested);
    m_sherpaManager->pauseIdleTimer();

    if (m_config.backend == AsrBackendKind::Sherpa) {
        bool initiated = m_sherpaManager->reloadModel(m_config);
        if (!initiated) {
            // 已加载且配置相同
            proceedToRecording();
        }
    } else {
        proceedToRecording();
    }
}

void WorkflowManager::proceedToRecording() {
    if (m_currentState != WorkflowState::Loading) return; 
    if (m_config.backend == AsrBackendKind::Sherpa && !m_sherpaManager->isModelLoaded()) {
        transitionTo(WorkflowState::Error, WorkflowEvent::ModelLoadFailed);
        LOG_ERROR("模型加载失败");
        return;
    }
    transitionTo(WorkflowState::Recording, WorkflowEvent::ModelLoaded);
    if (!m_recorder->startListening()) {
        transitionTo(WorkflowState::Error, WorkflowEvent::ErrorOccurred);
    }
}

void WorkflowManager::onModelLoadFinished(bool ok) {
    if (ok) LOG_INFO("模型加载完成");
    else LOG_ERROR("模型加载失败");
    if (m_currentState != WorkflowState::Loading) return;
    if (ok) proceedToRecording();
    else transitionTo(WorkflowState::Error, WorkflowEvent::ModelLoadFailed);
}

void WorkflowManager::onRecorderError(const QString& title, const QString& cause) {
    LOG_ERROR(QString("Recorder error: %1 %2").arg(title, cause));
    emit errorOccurred(title, cause);
}

void WorkflowManager::stopRecording() {
    if (m_currentState != WorkflowState::Recording &&
        m_currentState != WorkflowState::Transcribing &&
        m_currentState != WorkflowState::Processing &&
        m_currentState != WorkflowState::Loading) return;
    LOG_DEBUG("Stop Recording");

    transitionTo(WorkflowState::Stopping, WorkflowEvent::StopRequested);
    m_recorder->stopListening();
    m_sherpaManager->resumeIdleTimer();

    if (m_pending == 0) {
        transitionTo(WorkflowState::Idle, WorkflowEvent::AllTranscribed);
    }
}

void WorkflowManager::onUtteranceReady(const QByteArray& pcmData, int sampleRate) {
    LOG_DEBUG(tr("Utterance captured, size: %1 bytes").arg(pcmData.size()));
    m_pending++;
    transitionTo(WorkflowState::Transcribing, WorkflowEvent::UtteranceCaptured);
    m_transcription->transcribe(pcmData, sampleRate, m_config.audio.channels, m_config.audio.bitsPerSample);
}

void WorkflowManager::onUtteranceTranscribed(bool success, const QString& rawText, const QString& finalText, const QString& errorMsg) {
    if (m_pending > 0) m_pending--;

    if (success) {
        if (!finalText.isEmpty()) {
            LOG_DEBUG(QString("Transcription success: %1").arg(finalText));
            emit transcriptionResultReady(finalText);
            transitionTo(WorkflowState::Processing, WorkflowEvent::UtteranceTranscribed);
        }
    }
    else {
        emit errorOccurred(tr("语音识别失败"), errorMsg);
        LOG_WARN(QString("Transcription failed: %1").arg(errorMsg));
        transitionTo(WorkflowState::Processing, WorkflowEvent::UtteranceTranscribed);
    }

    if (m_pending == 0) {
        auto state = m_currentState == WorkflowState::Stopping ? WorkflowState::Recording : WorkflowState::Idle;
        //qDebug() << "m_pending" << static_cast<int>(state);
        transitionTo(state, WorkflowEvent::AllTranscribed);
    }else if (m_currentState != WorkflowState::Stopping) {
        transitionTo(WorkflowState::Processing, WorkflowEvent::UtteranceTranscribed);
    }
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
    if (!m_injectQueue.isEmpty()) {
        QMetaObject::invokeMethod(this, &WorkflowManager::processInjectQueue, Qt::QueuedConnection);
    }
}

bool WorkflowManager::canTransition(WorkflowState from, WorkflowEvent evt, WorkflowState &out) const {
    using S = WorkflowState; using E = WorkflowEvent;
    out = from;
    switch (evt) {
    case E::StartRequested:      if (from == S::Idle)        { out = S::Loading; return true; } break;
    case E::ModelLoaded:         if (from == S::Loading)     { out = S::Recording; return true; } break;
    case E::ModelLoadFailed:     if (from == S::Loading)     { out = S::Error; return true; } break;
    case E::UtteranceCaptured:   if (from == S::Recording)   { out = S::Transcribing; return true; }
                                 if (from == S::Processing)  { out = S::Transcribing; return true; }
                                 if (from == S::Stopping)    { out = S::Transcribing; return true; } break;
    case E::UtteranceTranscribed:if (from == S::Transcribing){ out = S::Processing; return true; }
                                 if (from == S::Processing)  { out = S::Processing; return true; } break;
    case E::AllTranscribed:      if (from == S::Stopping)    { out = S::Idle; return true; }   
                                 if (from == S::Idle)        { out = S::Idle; return true; }
                                 if (m_config.continuousMode && (from == S::Recording || from == S::Transcribing || from == S::Processing)) { out = S::Recording; return true;};
                                 if (from == S::Recording || from == S::Transcribing || from == S::Processing) { out = S::Idle; return true; } break;
    case E::StopRequested:       if (from == S::Recording || from == S::Transcribing || from == S::Processing || from == S::Loading) { out = S::Stopping; return true; } break;
    case E::ErrorOccurred:       { out = S::Error; return true; } break;
    }
    return false;
}

void WorkflowManager::transitionTo(WorkflowState newState, WorkflowEvent evt) {
    WorkflowState target = newState;
    if (!canTransition(m_currentState, evt, target)) {
        LOG_WARN(QString("Illegal transition ignored: %1 --%2--> %3")
                     .arg(static_cast<int>(m_currentState))
                     .arg(static_cast<int>(evt))
                     .arg(static_cast<int>(newState)));
        return;
    }
    if (m_currentState == target) return;
    m_currentState = target;
    emit stateChanged(m_currentState);
}
