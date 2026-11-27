#pragma once
#include <QAudioFormat>
#include <QByteArray>
#include <QObject>
#include <atomic>
#include <utility>

#include "config/AppConfig.h"
#include "interfaces/workflow_interfaces.h"

class QAudioSource;
class QIODevice;
class QThread;
class SpectrumWorker;
class SpeechSegmenter;

// @brief 麦克风录音服务：切句交给识别（utteranceReady），并给悬浮球提供音量和频谱。
// 麦克风 → 16k/单声道/Int16 → SpectrumWorker（频谱线程）；连续模式由 SpeechSegmenter（VAD 线程）按停顿断句，单句模式攒进 m_singleUtterance、停止时整段送出。
class AudioRecorderService : public IRecorder {
    Q_OBJECT
public:
    explicit AudioRecorderService(const AppConfig& config, QObject* parent = nullptr);
    ~AudioRecorderService() override;

    bool startListening() override;
    void stopListening() override;
    void pause() override;
    void resume() override;
    bool isListening() const override;
    bool isPaused() const override;

    void updateConfig() override;

    bool isVoiceActive() const;

    // 实时字幕的音频抽头，默认关闭；开启后每块 16k/单声道/Int16 数据通过 audioChunk 发出
    void setAudioTapEnabled(bool enabled) { m_audioTap = enabled; }
    // 最近一次交出的句子若是强制切段切出来的，返回切点之后留给下一句的毫秒数（取走后清零，否则为 0）。
    // 实时字幕换新流时据此补喂音频
    int takeCarryMs() { return std::exchange(m_carryMs, 0); }

    static QStringList availableMicrophones();

signals:
    void audioChunk(const QByteArray& pcm);

private slots:
    void onAudioDataReady();
    void onSpeechStarted();
    void onSpeechEnded();
    void onSentenceReady(const QByteArray& pcmData, int sampleRate);
    void onCarryOver(int carryMs) { m_carryMs = carryMs; }

private:
    bool ensureMicrophonePermission();
    bool ensureSegmenterReady();
    bool openMicrophone();

    void emitSingleUtterance();
    void emitUnfinishedSentences();

    void readAndDispatch();

    const AppConfig& m_config;

    QAudioSource* m_audioSource = nullptr;
    QIODevice* m_audioDevice = nullptr;
    QAudioFormat m_deviceFormat;         
    bool m_needsConversion = false;
    bool m_isPaused = false;
    std::atomic<bool> m_voiceActive{ false };
    std::atomic<bool> m_audioTap{ false };

    QByteArray m_singleUtterance;
    int m_carryMs = 0;

    SpectrumWorker* m_spectrumWorker = nullptr;
    QThread* m_spectrumThread = nullptr;
    SpeechSegmenter* m_segmenter = nullptr;
    QThread* m_segmenterThread = nullptr;

};
