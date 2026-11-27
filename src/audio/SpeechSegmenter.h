#pragma once
#include <QByteArray>
#include <QList>
#include <QObject>
#include <atomic>
#include <memory>
#include <vector>

#include "cxx-api.h"
#include "config/AppConfig.h"

// @brief 连续模式的断句器：把源源不断的麦克风音频切成一句一句，交给识别。
// 断句：静音达到「断句静音时长」时由 VAD 判句末；一直说不停则累计到 20 秒上限时，在最近 10 秒最长停顿处强切，
// 切点之后的音频作「切段残留」拼到下一句开头，不丢字；用户停止时 finishSession() 一并交出。
class SpeechSegmenter : public QObject {
    Q_OBJECT
public:
    explicit SpeechSegmenter(const AppConfig& config, int sampleRate, QObject* parent = nullptr);

    // 结束本次录音会话（须在本对象所在线程调用）：返回还没输出的语音段，并把状态清空，
    QList<QByteArray> finishSession();

    bool isReady() const { return m_ready.load(); }

public slots:
    void processChunk(const QByteArray chunk);
    void reloadDetector();

signals:
    void speechStarted();
    void speechEnded();
    void carryOver(int carryMs);
    void sentenceReady(const QByteArray& pcmData, int sampleRate);
    void errorOccurred(const QString& title, const QString& cause = {});

private:
    void feedDetector(const QByteArray& chunk);
    void reportSpeakingChange();
    void emitFinishedSentences();
    void emitLeftoverAfterCut();
    void cutIfTooLong();

    int64_t segmentLimitSamples() const;
    void cutAtPause(const std::vector<float>& segment);
    // 在 samples 末尾的一段范围内找切点：优先用短门槛 VAD 找停顿，找不到再找最安静处
    size_t findPauseCut(const std::vector<float>& samples, size_t searchFrom);
    size_t findQuietCut(const std::vector<float>& samples, size_t searchFrom) const;

    std::vector<float> takeWithLeftover(const std::vector<float>& segment);
    QByteArray toPcm16(const std::vector<float>& samples) const;
    void emitSentence(const std::vector<float>& samples, int64_t carrySamples = 0);
    void resetState();

    static constexpr int kMaxSegmentMs = 20000;
    static constexpr int kCutSearchMs = 10000;
    static constexpr int kPauseSilenceMs = 150;   // 找切点时，多长的静音算一次停顿

    const AppConfig& m_config;
    int m_sampleRate;

    std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> m_vad;
    std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> m_pauseVad;
    std::atomic<bool> m_ready{ false };                                   // m_vad 已创建，供录音线程查询

    bool m_isSpeaking = false;
    int64_t m_speechSamples = 0;
    int64_t m_samplesSinceCut = 0;
    std::vector<float> m_leftover;    // 切段残留：上次切点之后的音频，拼到下一句开头
};
