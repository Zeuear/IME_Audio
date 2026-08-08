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

    // 一句话缓冲完成，交给识别服务
    connect(m_recorder, &AudioRecorderService::utteranceReady, this, &WorkflowManager::onUtteranceReady);
    // 识别结果回调
    connect(m_transcription, &TranscriptionService::transcriptionFinished, this, &WorkflowManager::onUtteranceTranscribed);
    // 模型加载完成
    connect(m_sherpaManager, &SherpaManager::modelLoadFinished, this, &WorkflowManager::onModelLoadFinished);

    connect(this, &WorkflowManager::transcriptionResultReady, this, &WorkflowManager::onInjectText);
}

// ---- 门面命令 ----
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

// ---- 录音控制 ----
void WorkflowManager::startRecording() {
    if (m_currentState != WorkflowState::Idle) return;
    LOG_DEBUG("Start Recording");
    transitionTo(WorkflowState::Loading, WorkflowEvent::StartRequested);
    m_sherpaManager->pauseIdleTimer();

    if (m_config.backend == AsrBackendKind::Sherpa) {
        // 异步加载：loadModelAsync 入队，worker 完成后经 modelLoadFinished 驱动后续
        bool initiated = m_sherpaManager->reloadModel(m_config);
        if (!initiated) {
            // 已加载且配置相同，不会发 modelLoadFinished —— 直接继续
            proceedToRecording();
        }
        // initiated==true 时，onModelLoadFinished 会调用 proceedToRecording
    } else {
        proceedToRecording();
    }
}

void WorkflowManager::proceedToRecording() {
    if (m_currentState != WorkflowState::Loading) return;  // 防御：非 Loading 态不继续
    if (m_config.backend == AsrBackendKind::Sherpa && !m_sherpaManager->isModelLoaded()) {
        transitionTo(WorkflowState::Error, WorkflowEvent::ModelLoadFailed);
        LOG_ERROR("Model failed to load");
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
    // 只在 Loading 态响应（避免重复/延迟回调干扰）
    if (m_currentState != WorkflowState::Loading) return;
    if (ok) proceedToRecording();
    else transitionTo(WorkflowState::Error, WorkflowEvent::ModelLoadFailed);
}

void WorkflowManager::stopRecording() {
    if (m_currentState != WorkflowState::Recording &&
        m_currentState != WorkflowState::Transcribing &&
        m_currentState != WorkflowState::Processing &&
        m_currentState != WorkflowState::Loading) return;
    LOG_DEBUG("Stop Recording");

    transitionTo(WorkflowState::Stopping, WorkflowEvent::StopRequested);
    m_recorder->stopListening();
    // 结束监听：恢复空闲计时
    m_sherpaManager->resumeIdleTimer();

    // 冲刷：若没有待转录句，立即进入 Idle；否则等最后一句转录完由 AllTranscribed 落 Idle
    if (m_pending == 0) {
        transitionTo(WorkflowState::Idle, WorkflowEvent::AllTranscribed);
    }
}

// ---- 转写流水线 ----
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
        }
    }
    else {
        QMessageBox::warning(nullptr, tr("Error"), errorMsg);
        LOG_WARN(QString("Transcription failed: %1").arg(errorMsg));
        LOG_WARN(errorMsg);
    }

    if (m_pending == 0) {
        // 冲刷完最后一句：依当前是否 Stopping 决定落点（单一事实源 = 状态机）
        transitionTo(m_currentState == WorkflowState::Stopping ? WorkflowState::Idle : WorkflowState::Recording,
                     WorkflowEvent::AllTranscribed);
    }
    else if (m_currentState != WorkflowState::Stopping) {
        // 仍有待转录句且未在停止冲刷中：进入 Processing 等待下一句
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
    // 递归处理队列里剩余的内容(注入之间保留处理顺序)
    if (!m_injectQueue.isEmpty()) {
        QMetaObject::invokeMethod(this, &WorkflowManager::processInjectQueue, Qt::QueuedConnection);
    }
}

// ---- 集中式状态机 ----
bool WorkflowManager::canTransition(WorkflowState from, WorkflowEvent evt, WorkflowState &out) const {
    using S = WorkflowState; using E = WorkflowEvent;
    // 非法/默认
    out = from;
    switch (evt) {
    case E::StartRequested:      if (from == S::Idle)        { out = S::Loading; return true; } break;
    case E::ModelLoaded:         if (from == S::Loading)     { out = S::Recording; return true; } break;
    case E::ModelLoadFailed:     if (from == S::Loading)     { out = S::Error; return true; } break;
    case E::UtteranceCaptured:   if (from == S::Recording)   { out = S::Transcribing; return true; }
                                  if (from == S::Processing)  { out = S::Transcribing; return true; } break;
    case E::UtteranceTranscribed:if (from == S::Transcribing){ out = S::Processing; return true; }
                                  if (from == S::Processing)  { out = S::Processing; return true; } break;
    case E::AllTranscribed:      if (from == S::Stopping)    { out = S::Idle; return true; }
                                  if (from == S::Recording || from == S::Transcribing || from == S::Processing) { out = S::Recording; return true; } break;
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
