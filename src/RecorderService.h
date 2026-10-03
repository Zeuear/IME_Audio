#pragma once
#include <QAudioFormat>
#include <QByteArray>
#include <QObject>
#include <atomic>

#include "AppConfig.h"
#include "interfaces/workflow_interfaces.h"
#include "utils/SystemAudioEndpointController.h"

class QAudioSink;
class QAudioSource;
class QBuffer;
class QIODevice;
class QThread;
class SpectrumWorker;
class SpeechSegmenter;

// @brief 麦克风录音服务：采集音频，切成一句一句交给识别（utteranceReady），
// 同时给悬浮球提供音量和频谱。
//
// 音频走向（每收到一块麦克风数据）：
//
//   麦克风 → 统一成 16k/单声道/Int16 ─┬→ SpectrumWorker（频谱线程）→ 悬浮球动画
//                                     │
//                                     ├→ 连续模式：SpeechSegmenter（VAD 线程）按停顿断句 → utteranceReady
//                                     └→ 单句模式：攒在 m_singleUtterance，按停止时整段 → utteranceReady
//
class AudioRecorderService : public IRecorder {
    Q_OBJECT
public:
    explicit AudioRecorderService(const AppConfig& config, QObject* parent = nullptr);
    ~AudioRecorderService() override;

    // ---- 录音控制 ----
    bool startListening() override;
    void stopListening() override;
    void pause() override;
    void resume() override;
    bool isListening() const override;
    bool isPaused() const override;

    void updateConfig() override;

    bool isVoiceActive() const;

    static QStringList availableMicrophones();
    static QStringList availableSpeakers();

public slots:
    void playTestTone() override;

private slots:
    void onAudioDataReady();
    void onSpeechStarted();
    void onSpeechEnded();
    void onSentenceReady(const QByteArray& pcmData, int sampleRate);

private:
    // ---- 开始录音的步骤 ----
    // 开录之前确认麦克风授权。
    bool ensureMicrophonePermission();
    bool openMicrophone();

    // ---- 停止录音时交出最后一句 ----
    // 单句模式
    void emitSingleUtterance();
    // 连续模式
    void emitUnfinishedSentences();

    // 读出设备缓冲里的数据，统一格式后分发给频谱和断句
    void readAndDispatch();

    const AppConfig& m_config;

    // 采集
    QAudioSource* m_audioSource = nullptr;
    QIODevice* m_audioDevice = nullptr;
    QAudioFormat m_deviceFormat;         
    bool m_needsConversion = false;
    bool m_isPaused = false;
    std::atomic<bool> m_voiceActive{ false };

    QByteArray m_singleUtterance;

    // 后台线程
    SpectrumWorker* m_spectrumWorker = nullptr;
    QThread* m_spectrumThread = nullptr;
    SpeechSegmenter* m_segmenter = nullptr;
    QThread* m_segmenterThread = nullptr;

    // 测试音播放
    QAudioSink* m_audioSink = nullptr;
    QBuffer* m_audioBuffer = nullptr;

    SystemAudioEndpointController m_endpointController;
};
