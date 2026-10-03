#pragma once
#include <QByteArray>
#include <QList>
#include <QObject>
#include <atomic>
#include <memory>
#include <vector>

#include "cxx-api.h"
#include "AppConfig.h"

// @brief 连续模式的断句器：把源源不断的麦克风音频切成一句一句，交给识别。
//
// 一句话什么时候算结束：
//   1. 正常情况：说完后静音达到「断句静音时长」，VAD 判定句末，整句输出。
//   2. 一直说不停：累计到分段上限（20 秒）时强制切一刀。切点选在最近 10 秒里最长的停顿处，
//      切点之后的音频留作「切段残留」，拼到下一句开头，所以不会丢字。
//   3. 用户按停止：finishSession() 把还没说完的半句和切段残留一并交出，并清空状态。
class SpeechSegmenter : public QObject {
    Q_OBJECT
public:
    explicit SpeechSegmenter(const AppConfig& config, int sampleRate, QObject* parent = nullptr);

    // 结束本次录音会话（须在本对象所在线程调用）：返回还没输出的语音段，并把状态清空，
    QList<QByteArray> finishSession();

    // VAD 是否已创建成功
    bool isReady() const { return m_ready.load(); }

public slots:
    void processChunk(const QByteArray chunk);
    // 按当前配置重新创建 VAD（阈值、断句静音等设置变更后调用）
    void reloadDetector();

signals:
    void speechStarted();
    void speechEnded();
    void sentenceReady(const QByteArray& pcmData, int sampleRate);
    void errorOccurred(const QString& title, const QString& cause = {});

private:
    // ---- processChunk 的各个步骤，按执行顺序 ----
    void feedDetector(const QByteArray& chunk);
    void reportSpeakingChange();
    void emitFinishedSentences();
    void emitLeftoverAfterCut();
    void cutIfTooLong();

    // ---- 强制切段 ----
    int64_t segmentLimitSamples() const;
    // 切段残留 + segment 拼起来，在停顿处切开：前半句输出，后半段成为新的切段残留
    void cutAtPause(const std::vector<float>& segment);
    // 在 samples 末尾的一段范围内找切点：优先用短门槛 VAD 找停顿，找不到再找最安静处
    size_t findPauseCut(const std::vector<float>& samples, size_t searchFrom);
    size_t findQuietCut(const std::vector<float>& samples, size_t searchFrom) const;

    // 把切段残留拼到 segment 前面，残留随之清空
    std::vector<float> takeWithLeftover(const std::vector<float>& segment);
    QByteArray toPcm16(const std::vector<float>& samples) const;
    void emitSentence(const std::vector<float>& samples);
    void resetState();

    static constexpr int kMaxSegmentMs = 20000;   // 一直说不停时的分段上限
    static constexpr int kCutSearchMs = 10000;    // 在分段末尾多长范围内找切点
    static constexpr int kPauseSilenceMs = 150;   // 找切点时，多长的静音算一次停顿

    const AppConfig& m_config;
    int m_sampleRate;

    std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> m_vad;       // 断句用，门槛 = 用户设置
    std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> m_pauseVad;  // 找切点用，门槛 150 ms
    std::atomic<bool> m_ready{ false };                                   // m_vad 已创建，供录音线程查询

    bool m_isSpeaking = false;
    int64_t m_speechSamples = 0;      // 当前这句已连续说了多少样本
    int64_t m_samplesSinceCut = 0;    // 距上次强制切段过了多少样本
    std::vector<float> m_leftover;    // 切段残留：上次切点之后的音频，拼到下一句开头
};
