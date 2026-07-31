#pragma once
#include <QObject>
#include <QAudioSource>
#include <QIODevice>
#include "AppConfig.h"

#include "cxx-api.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "kiss_fft.h"
#include "kiss_fftr.h"

#ifdef __cplusplus
}
#endif


class SpectrumWorker : public QObject {
    Q_OBJECT
public:
    explicit SpectrumWorker(int sampleRate);
    ~SpectrumWorker();

public slots:
    void processChunk(const QByteArray chunk);
    void updateVadState(const QByteArray& chunk);
    void resetLevel();

signals:
    void spectrumReady(const QVector<float>& bands);
    void levelUpdated(float rmsLevel);

private:
    static constexpr int kFftSize = 512;
    static constexpr int kBandCount = 16;
    void* m_kissFftCfg = nullptr;
    float m_rmsLevel = 0;
    float m_peakLevel = 0;

    std::vector<float> m_fftInputBuffer;
    int m_sampleRate;
};


class VadWorker : public QObject {
    Q_OBJECT
public:
    explicit VadWorker(const AppConfig& config, int sampleRate, QObject* parent = nullptr);

public slots:
    void processChunk(const QByteArray chunk);
    void reset();
    void rebuildDetector();

signals:
    void speechStarted();
    void speechSegmentReady(const QByteArray& pcmData, int sampleRate); 
    void speechEnded(); 

private:
    std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> m_vad;
    int m_sampleRate;
    bool m_wasSpeaking = false;
    const AppConfig& m_config;
};

class AudioRecorderService: public QObject {
    Q_OBJECT
public:
    struct RuntimeStatus {
        bool isListening = false;
        bool isPaused = false;
        bool hadVoice = false;
        int currentSegmentMs = 0;
    };

    explicit AudioRecorderService(const AppConfig& config, QObject *parent = nullptr);
    ~AudioRecorderService() override;

    bool startListening();
    void stopListening();
    bool isListening() const;
    bool isPaused() const;

    void pause();
    void resume();
    RuntimeStatus runtimeStatus() const;

    static QStringList availableMicrophones();
    void updateConfig();

signals:
    void utteranceReady(const QByteArray& pcmData, int sampleRate);

    // 波形动画/UI 状态展示用
    void levelUpdated(float rmsNormalized);
    void voiceStarted();
    void voiceStopped();
    void spectrumUpdated(const QVector<float>& bands); 

private slots:
    void onAudioDataReady();
    void onVadSpeechStarted();
    void onVadSpeechEnded();
    void onVadSegmentReady(const QByteArray& pcmData, int sampleRate);

private:
    void finalizeSegmentIfNeeded(bool forceCut);
    int bytesPerMs() const;

    QAudioSource *m_audioSource = nullptr;
    QIODevice *m_audioDevice = nullptr;

    RuntimeStatus m_status;
    const AppConfig& m_config;

    QByteArray m_segmentBuffer;   
    qint64 m_silenceAccumMs = 0;

    bool m_manualActive = false;
    bool m_autoStopEnabled = true;

    SpectrumWorker* m_spectrumWorker;
    QThread* m_spectrumThread = nullptr;

    VadWorker* m_vadWorker = nullptr;
    QThread* m_vadThread = nullptr;

    QByteArray m_preRollBuffer;
    static constexpr int kPreRollMs = 300;

    // 整段录音原始 PCM，仅用于调试导出
    QByteArray m_fullSessionBuffer;   
    bool writeWavFile(const QString& filePath, const QByteArray& pcmData,
        int sampleRate, int channels, int bitsPerSample) const;
};