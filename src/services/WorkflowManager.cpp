#include "WorkflowManager.h"
#include "utils/Logger.h"
#include "InputInjector.h"
#include "utils/PlatformPermissions.h"
#include <QMetaObject>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include "utils/AppPaths.h"

// 流程：加载模型 → 录音 → 转录（非 continue 模式是停止后转录，转完关窗；continue 模式转完继续录音，无声则自动停止）→ 打印

namespace {

// 排查识别效果用：设置环境变量 IMEAUDIO_DUMP_UTTERANCES=1 后，每句送去识别的音频都存成 WAV，
// 可以直接听 VAD 切出来的句子是否完整（句首被吃、切在词中间等）。默认关闭，不影响正常使用
bool dumpUtterancesEnabled() {
    static const bool enabled = qEnvironmentVariableIntValue("IMEAUDIO_DUMP_UTTERANCES") != 0;
    return enabled;
}

void dumpUtterance(quint64 id, const QByteArray& pcm, int sampleRate, int channels, int bitsPerSample) {
    const QString dir = QDir(AppPaths::dataDir()).absoluteFilePath("debug_utterances");
    QDir().mkpath(dir);
    const QString path = QDir(dir).absoluteFilePath(
        QString("%1_%2.wav").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz")).arg(id));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_WARN(QString("Dump utterance failed: %1").arg(path));
        return;
    }
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    const quint32 dataSize = static_cast<quint32>(pcm.size());
    file.write("RIFF");
    out << quint32(36 + dataSize);
    file.write("WAVEfmt ");
    out << quint32(16) << quint16(1) << quint16(channels) << quint32(sampleRate)
        << quint32(sampleRate * channels * bitsPerSample / 8) << quint16(channels * bitsPerSample / 8)
        << quint16(bitsPerSample);
    file.write("data");
    out << dataSize;
    file.write(pcm);
    LOG_DEBUG(QString("Utterance #%1 dumped: %2 (%3 s)").arg(id).arg(path)
                  .arg(double(pcm.size()) / (sampleRate * channels * bitsPerSample / 8), 0, 'f', 2));
}

}

WorkflowManager::WorkflowManager(const AppConfig &config, QObject *parent) : QObject(parent), m_config(config) {}

void WorkflowManager::initialize(IRecorder *recorder,
                                ITranscription *transcription,
                                ISherpaModel* sherpManager)
{
    m_recorder = recorder;
    m_transcription = transcription;
    m_sherpaManager = sherpManager;

    connect(m_sherpaManager, &ISherpaModel::modelLoadFinished, this, &WorkflowManager::onModelLoadFinished);
    connect(m_recorder, &IRecorder::errorOccurred, this, &WorkflowManager::onRecorderError);
    connect(m_recorder, &IRecorder::utteranceReady, this, &WorkflowManager::onUtteranceReady);
    connect(m_transcription, &ITranscription::transcriptionFinished, this, &WorkflowManager::onUtteranceTranscribed);
    connect(m_transcription, &ITranscription::transcriptionPending, this, &WorkflowManager::utterancePending);
    connect(m_transcription, &ITranscription::transcriptionTranslated, this, &WorkflowManager::utteranceTranslated);
    connect(this, &WorkflowManager::transcriptionResultReady, this, &WorkflowManager::onInjectText);
}

void WorkflowManager::start() { startRecording(); }
void WorkflowManager::stop() { stopRecording(); }

// 停止时录音层会同步交出最后一段音频，置标志让 onUtteranceReady 丢掉它
void WorkflowManager::cancel()
{
    m_discarding = true;
    stopRecording();
    m_discarding = false;
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
    m_stopRequested = false;
    transitionTo(WorkflowState::Loading, WorkflowEvent::StartRequested);
    m_sherpaManager->pauseIdleTimer();

    if (!m_recorder->startListening()) {
        abortRecording(WorkflowEvent::ErrorOccurred);
        return;
    }

    if (m_config.backend == AsrBackendKind::Sherpa) {
        bool initiated = m_sherpaManager->reloadModel(m_config);
        if (!initiated) {
            proceedToRecording();
        }
    } else {
        proceedToRecording();
    }
}

void WorkflowManager::proceedToRecording() {
    if (m_currentState != WorkflowState::Loading) return; 
    if (m_config.backend == AsrBackendKind::Sherpa && !m_sherpaManager->isModelLoaded()) {
        LOG_ERROR("Model load failed");
        abortRecording(WorkflowEvent::ModelLoadFailed);
        return;
    }
    transitionTo(WorkflowState::Recording, WorkflowEvent::ModelLoaded);
}

void WorkflowManager::abortRecording(WorkflowEvent evt) {
    transitionTo(WorkflowState::Error, evt);
    m_recorder->stopListening();
    m_sherpaManager->resumeIdleTimer();
    m_pending = 0;
    m_utteranceIds.clear();
    transitionTo(WorkflowState::Idle, WorkflowEvent::ErrorHandled);
}

void WorkflowManager::onModelLoadFinished(bool ok) {
    if (ok) LOG_INFO("Model load completed");
    else LOG_ERROR("Model load failed");
    if (m_currentState != WorkflowState::Loading) return;
    if (ok) proceedToRecording();
    else abortRecording(WorkflowEvent::ModelLoadFailed);
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

    m_stopRequested = true;
    transitionTo(WorkflowState::Stopping, WorkflowEvent::StopRequested);
    m_recorder->stopListening();
    m_sherpaManager->resumeIdleTimer();

    if (m_pending == 0) {
        transitionTo(WorkflowState::Idle, WorkflowEvent::AllTranscribed);
    }
}

void WorkflowManager::onUtteranceReady(const QByteArray& pcmData, int sampleRate) {
    if (m_currentState == WorkflowState::Idle || m_currentState == WorkflowState::Error) {
        LOG_WARN(QString("Utterance dropped, not recording (%1 bytes)").arg(pcmData.size()));
        return;
    }
    if (m_discarding) {
        LOG_DEBUG(QString("Utterance discarded by cancel (%1 bytes)").arg(pcmData.size()));
        return;
    }
    LOG_DEBUG(QString("Utterance captured, size: %1 bytes").arg(pcmData.size()));

    m_pending++;
    const quint64 id = ++m_nextUtteranceId;
    m_utteranceIds.enqueue(id);
    emit utteranceAccepted(id);
    if (dumpUtterancesEnabled()) dumpUtterance(id, pcmData, sampleRate, m_config.audio.channels, m_config.audio.bitsPerSample);
    transitionTo(WorkflowState::Transcribing, WorkflowEvent::UtteranceCaptured);
    m_transcription->transcribe(pcmData, sampleRate, m_config.audio.channels, m_config.audio.bitsPerSample);
}

void WorkflowManager::onUtteranceTranscribed(bool success, const QString& rawText, const QString& finalText, const QString& errorMsg) {
    if (m_pending > 0) m_pending--;
    const quint64 id = m_utteranceIds.isEmpty() ? 0 : m_utteranceIds.dequeue();
    emit utteranceFinished(success ? finalText : QString(), id);
    if (dumpUtterancesEnabled())
        LOG_DEBUG(QString("Utterance #%1 raw=\"%2\" final=\"%3\"").arg(id).arg(rawText, finalText));

    if (success) {
        if (!finalText.isEmpty()) {
            LOG_DEBUG(QString("Transcription success: %1").arg(finalText));
            emit transcriptionResultReady(finalText);
            transitionTo(WorkflowState::Processing, WorkflowEvent::UtteranceTranscribed);
        }
        else {
            LOG_WARN(QString("Transcription returned empty text (rawText=\"%1\")").arg(rawText));
        }
    }
    else {
        emit errorOccurred(tr("语音识别失败"), errorMsg);
        LOG_WARN(QString("Transcription failed: %1").arg(errorMsg));
        transitionTo(WorkflowState::Processing, WorkflowEvent::UtteranceTranscribed);
    }

    if (m_pending == 0) {
        auto state = m_currentState == WorkflowState::Stopping ? WorkflowState::Recording : WorkflowState::Idle;
        transitionTo(state, WorkflowEvent::AllTranscribed);
    }else if (m_currentState != WorkflowState::Stopping) {
        transitionTo(WorkflowState::Processing, WorkflowEvent::UtteranceTranscribed);
    }
}

void WorkflowManager::onInjectText(const QString& text) {
    if (text.isEmpty()) return;
    // 过滤器要先于开关：Google 文档输出写的是被绑定的标签页，不是前台窗口，
    // "输入到前台"关掉时它仍应收到结果
    if (m_injectFilter && m_injectFilter(text)) return;
    if (!m_injectEnabled) {
        LOG_DEBUG("Inject skipped: inject to foreground is off");
        return;
    }
    m_injectQueue.enqueue(text);
    processInjectQueue();
}

void WorkflowManager::processInjectQueue() {
    if (m_injecting || m_injectQueue.isEmpty()) return;

    m_injecting = true;
    QString text = m_injectQueue.dequeue();

    bool ok = InputInjector::inject(text, InputInjector::defaultMode());
    if (!ok) {
        LOG_WARN(QString("InputInjector failed (%1 chars)").arg(text.size()));
        if (PlatformPermissions::accessibilityStatus() == PlatformPermissions::Status::Denied) {
            emit errorOccurred(tr("文本注入失败"),
                               tr("请在「系统设置 → 隐私与安全性 → 辅助功能」中允许本应用；转录文本已留在剪贴板，可直接 ⌘V 粘贴"));
        }
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
                                 if (m_config.continuousMode && !m_stopRequested && (from == S::Recording || from == S::Transcribing || from == S::Processing)) { out = S::Recording; return true;};
                                 if (from == S::Recording || from == S::Transcribing || from == S::Processing) { out = S::Idle; return true; } break;
    case E::StopRequested:       if (from == S::Recording || from == S::Transcribing || from == S::Processing || from == S::Loading) { out = S::Stopping; return true; } break;
    case E::ErrorOccurred:       { out = S::Error; return true; } break;
    case E::ErrorHandled:        if (from == S::Error)       { out = S::Idle; return true; } break;
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
