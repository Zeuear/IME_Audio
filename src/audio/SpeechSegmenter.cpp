#include "SpeechSegmenter.h"

#include <QFile>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <limits>

#include "utils/AppPaths.h"
#include "utils/Diagnostics.h"
#include "utils/Logger.h"

SpeechSegmenter::SpeechSegmenter(const AppConfig& config, int sampleRate, QObject* parent)
    : QObject(parent), m_config(config), m_sampleRate(sampleRate)
{}

void SpeechSegmenter::reloadDetector()
{
    // vadPath 是构造配置时解析的；VAD 模型可能是之后才下载好的，这里重新解析一次
    QString vadPath = m_config.sherpa.vadPath;
    if (!QFile::exists(vadPath)) {
        vadPath = AppPaths::vadModelFile();
    }
    if (!QFile::exists(vadPath)) {
        LOG_ERROR(QString("VAD model not found: %1").arg(vadPath));
        emit errorOccurred(tr("录音启动失败"), tr("VAD 模型缺失，正在自动下载，请稍候重试"));
        return;
    }

    sherpa_onnx::cxx::VadModelConfig vadConfig;
    vadConfig.silero_vad.model = vadPath.toStdString();
    vadConfig.silero_vad.threshold = static_cast<float>(m_config.audio.voiceThreshold) / 1000;
    vadConfig.silero_vad.min_silence_duration = static_cast<float>(m_config.audio.silenceTimeoutMs) / 1000;
    vadConfig.silero_vad.min_speech_duration = static_cast<float>(m_config.audio.minRecordMs) / 1000;
    // 比我们自己的分段上限多留 5 秒：强制切段由 cutIfTooLong 负责，
    // 不能让 VAD 先到顶、按它自己的规则硬切
    const int segmentLimitMs = qMin(m_config.audio.maxRecordMs, kMaxSegmentMs);
    vadConfig.silero_vad.max_speech_duration = static_cast<float>(segmentLimitMs) / 1000 + 5.0f;
    vadConfig.sample_rate = m_config.audio.sampleRate;

    Diagnostics::logFileState("VAD | model", vadPath);
    sherpa_onnx::cxx::VadModelConfig pauseConfig = vadConfig;
    pauseConfig.silero_vad.min_silence_duration = static_cast<float>(kPauseSilenceMs) / 1000;
    pauseConfig.silero_vad.min_speech_duration = 0.1f;

    std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> newVad;
    try {
        newVad = std::make_unique<sherpa_onnx::cxx::VoiceActivityDetector>(
            sherpa_onnx::cxx::VoiceActivityDetector::Create(vadConfig, static_cast<float>(m_config.audio.maxRecordMs) / 1000.0f + 5.0f));
        m_pauseVad = std::make_unique<sherpa_onnx::cxx::VoiceActivityDetector>(
            sherpa_onnx::cxx::VoiceActivityDetector::Create(pauseConfig, pauseConfig.silero_vad.max_speech_duration));
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("VAD | create failed: %1").arg(e.what()));
        emit errorOccurred(tr("录音启动失败"), tr("VAD 模型加载失败，详见日志"));
        return;
    }

    m_vad = std::move(newVad);
    m_speechSamples = 0;
    m_leftover.clear();
    LOG_DEBUG(QString("VAD Model update successful (threshold=%1, silence=%2ms, sampleRate=%3)")
                  .arg(vadConfig.silero_vad.threshold).arg(m_config.audio.silenceTimeoutMs).arg(m_config.audio.sampleRate));
}

// ============================================================
// 每块音频的处理流程
// ============================================================
void SpeechSegmenter::processChunk(const QByteArray chunk)
{
    if (!m_vad) return;
    const int count = chunk.size() / 2;

    feedDetector(chunk);
    reportSpeakingChange();
    emitFinishedSentences();

    m_samplesSinceCut += count;
    emitLeftoverAfterCut();

    m_speechSamples = m_isSpeaking ? m_speechSamples + count : 0;
    cutIfTooLong();
}

void SpeechSegmenter::feedDetector(const QByteArray& chunk)
{
    const int16_t* samples = reinterpret_cast<const int16_t*>(chunk.constData());
    const int count = chunk.size() / 2;

    std::vector<float> floatSamples(count);
    for (int i = 0; i < count; ++i) {
        floatSamples[i] = samples[i] / 32768.0f;
    }
    m_vad->AcceptWaveform(floatSamples.data(), count);
}

// 开始说话 / 停止说话 通知界面（悬浮球动画）
void SpeechSegmenter::reportSpeakingChange()
{
    const bool speaking = m_vad->IsDetected();
    if (speaking && !m_isSpeaking) {
        emit speechStarted();
    } else if (!speaking && m_isSpeaking) {
        emit speechEnded();
    }
    m_isSpeaking = speaking;
}

// VAD 判定句末的整句，前面拼上切段残留后输出
void SpeechSegmenter::emitFinishedSentences()
{
    while (!m_vad->IsEmpty()) {
        emitSentence(takeWithLeftover(m_vad->Front().samples));
        m_vad->Pop();
    }
}

// 切点之后语音没有延续（刚好停在切点附近），切段残留单独成句，避免丢字。
// 等 0.1 秒是为了让 VAD 至少处理过一个窗口，否则 IsDetected 还没来得及恢复
void SpeechSegmenter::emitLeftoverAfterCut()
{
    if (!m_leftover.empty() && !m_isSpeaking && m_samplesSinceCut >= m_sampleRate / 10) {
        emitSentence(m_leftover);
        m_leftover.clear();
    }
}

// 一直说不停、累计到分段上限：让 VAD 交出当前语音，在停顿处切开
void SpeechSegmenter::cutIfTooLong()
{
    if (!m_isSpeaking) return;
    if (static_cast<int64_t>(m_leftover.size()) + m_speechSamples < segmentLimitSamples()) return;

    m_vad->Flush();
    while (!m_vad->IsEmpty()) {
        cutAtPause(m_vad->Front().samples);
        m_vad->Pop();
    }
}

// ============================================================
// 强制切段
// ============================================================
int64_t SpeechSegmenter::segmentLimitSamples() const
{
    const int segmentLimitMs = qMin(m_config.audio.maxRecordMs, kMaxSegmentMs);
    return static_cast<int64_t>(segmentLimitMs) * m_sampleRate / 1000;
}

void SpeechSegmenter::cutAtPause(const std::vector<float>& segment)
{
    std::vector<float> all = takeWithLeftover(segment);

    // 搜索范围不超过上限的一半：最长录音设得很短（如 3 秒）时，若整段都可搜，
    // 切点可能落在段首，留下的残留一进来就又满上限，会连续切出一串碎段
    const size_t searchLen = static_cast<size_t>(
        qMin(static_cast<int64_t>(kCutSearchMs) * m_sampleRate / 1000, segmentLimitSamples() / 2));
    const size_t searchFrom = all.size() > searchLen ? all.size() - searchLen : 0;
    const size_t cut = findPauseCut(all, searchFrom);

    m_leftover.assign(all.begin() + cut, all.end());
    all.resize(cut);
    m_speechSamples = 0;
    m_samplesSinceCut = 0;
    LOG_DEBUG(QString("VAD | no pause within limit, cut at %1 s, carry %2 s")
                  .arg(double(cut) / m_sampleRate, 0, 'f', 1)
                  .arg(double(m_leftover.size()) / m_sampleRate, 0, 'f', 1));
    emitSentence(all);
}

size_t SpeechSegmenter::findPauseCut(const std::vector<float>& samples, size_t searchFrom)
{
    // 主 VAD 的静音门槛是用户设置的断句时长，看不到比它短的停顿；
    // 用一个短门槛的 VAD 重扫整段找停顿
    if (!m_pauseVad) return findQuietCut(samples, searchFrom);

    // Reset 会把内部环形缓冲的下标归零，段的 start 就是相对 samples 开头的下标
    constexpr int kFeed = 512;
    m_pauseVad->Reset();
    for (size_t i = 0; i < samples.size(); i += kFeed) {
        const int n = static_cast<int>(qMin<size_t>(kFeed, samples.size() - i));
        m_pauseVad->AcceptWaveform(samples.data() + i, n);
    }
    m_pauseVad->Flush();

    // 相邻两段之间的空隙就是停顿；取搜索范围内最长的，越靠后略加偏好（每秒折算 20 ms）
    int64_t prevEnd = -1;
    size_t bestCut = 0;
    double bestScore = 0.0;
    while (!m_pauseVad->IsEmpty()) {
        const auto seg = m_pauseVad->Front();
        const int64_t start = seg.start;
        if (prevEnd >= 0 && start > prevEnd) {
            const size_t mid = static_cast<size_t>((prevEnd + start) / 2);
            if (mid >= searchFrom && mid < samples.size()) {
                const double score = double(start - prevEnd) + 0.02 * double(mid - searchFrom);
                if (score > bestScore) {
                    bestScore = score;
                    bestCut = mid;
                }
            }
        }
        prevEnd = start + static_cast<int64_t>(seg.samples.size());
        m_pauseVad->Pop();
    }

    if (bestCut == 0) {
        LOG_DEBUG("VAD | no pause found by VAD, falling back to energy");
        return findQuietCut(samples, searchFrom);
    }
    LOG_DEBUG(QString("VAD | pause at %1 s").arg(double(bestCut) / m_sampleRate, 0, 'f', 2));
    return bestCut;
}

size_t SpeechSegmenter::findQuietCut(const std::vector<float>& samples, size_t searchFrom) const
{
    // 20 ms 一帧算能量，再用 200 ms 滑窗取平均，字与字之间的短暂间隙不会被当成停顿
    const size_t frame = static_cast<size_t>(m_sampleRate / 50);
    constexpr size_t kWinFrames = 10;
    const size_t frames = samples.size() / frame;
    const size_t first = searchFrom / frame;
    if (frame == 0 || frames < first + kWinFrames) {
        return samples.size();
    }

    std::vector<float> db(frames);
    for (size_t f = 0; f < frames; ++f) {
        double sumSq = 0.0;
        for (size_t i = f * frame; i < (f + 1) * frame; ++i) {
            sumSq += double(samples[i]) * samples[i];
        }
        db[f] = 10.0f * static_cast<float>(std::log10(std::max(sumSq / frame, 1e-12)));
    }

    float winSum = 0.0f;
    for (size_t f = first; f < first + kWinFrames; ++f) {
        winSum += db[f];
    }
    size_t bestFrame = first;
    float bestScore = std::numeric_limits<float>::max();
    for (size_t f = first; f + kWinFrames <= frames; ++f) {
        if (f > first) {
            winSum += db[f + kWinFrames - 1] - db[f - 1];
        }
        // 越靠后切，本段越完整，每秒给 0.5 dB 的偏好；
        // 真正的停顿通常比语音低 20 dB 以上，这点偏好不会盖过它
        const float seconds = float((f - first) * frame) / m_sampleRate;
        const float score = winSum / kWinFrames - 0.5f * seconds;
        if (score <= bestScore) {
            bestScore = score;
            bestFrame = f;
        }
    }
    return (bestFrame + kWinFrames / 2) * frame;
}

// ============================================================
// 结束会话
// ============================================================
QList<QByteArray> SpeechSegmenter::finishSession()
{
    QList<QByteArray> out;
    if (!m_vad) return out;

    // Flush 把正在检测的语音（含还没等够的断句静音）作为一段交出来
    m_vad->Flush();
    while (!m_vad->IsEmpty()) {
        out.append(toPcm16(takeWithLeftover(m_vad->Front().samples)));
        m_vad->Pop();
    }
    if (!m_leftover.empty()) {
        out.append(toPcm16(m_leftover));
    }

    qint64 totalBytes = 0;
    for (const QByteArray& pcm : out) totalBytes += pcm.size();
    LOG_DEBUG(QString("VAD | finish session: %1 segment(s), %2 s")
                  .arg(out.size()).arg(double(totalBytes / 2) / m_sampleRate, 0, 'f', 1));

    resetState();
    return out;
}

void SpeechSegmenter::resetState()
{
    m_vad->Reset();
    m_leftover.clear();
    m_speechSamples = 0;
    m_samplesSinceCut = 0;
    if (m_isSpeaking) {
        m_isSpeaking = false;
        emit speechEnded();
    }
}

// ============================================================
// 小工具
// ============================================================
std::vector<float> SpeechSegmenter::takeWithLeftover(const std::vector<float>& segment)
{
    std::vector<float> all = std::move(m_leftover);
    m_leftover.clear();
    all.insert(all.end(), segment.begin(), segment.end());
    return all;
}

QByteArray SpeechSegmenter::toPcm16(const std::vector<float>& samples) const
{
    std::vector<int16_t> pcm16(samples.size());
    for (size_t i = 0; i < samples.size(); ++i) {
        float v = qBound(-1.0f, samples[i], 1.0f);
        pcm16[i] = static_cast<int16_t>(v * 32767.0f);
    }
    return QByteArray(reinterpret_cast<const char*>(pcm16.data()),
        static_cast<int>(pcm16.size() * sizeof(int16_t)));
}

void SpeechSegmenter::emitSentence(const std::vector<float>& samples)
{
    if (samples.empty()) return;
    LOG_DEBUG(QString("VAD | segment %1 s").arg(double(samples.size()) / m_sampleRate, 0, 'f', 1));
    emit sentenceReady(toPcm16(samples), m_sampleRate);
}
