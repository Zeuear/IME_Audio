#pragma once
#include <QObject>
#include <QAudioSource>
#include <QIODevice>
#include "AppConfig.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "kiss_fft.h"
#include "kiss_fftr.h"

#ifdef __cplusplus
}
#endif

class AudioRecorderService : public QObject {
    Q_OBJECT
public:
    struct RuntimeStatus {
        bool isListening = false;
        bool isPaused = false;
        bool hadVoice = false;
        int peakLevel = 0;
        float rmsLevel = 0.0f;
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

signals:
    void utteranceReady(const QByteArray& pcmData, int sampleRate);

    // 波形动画/UI 状态展示用
    void levelUpdated(float rmsNormalized);
    void voiceStarted();
    void voiceStopped();
    void spectrumUpdated(const QVector<float>& bands); 

private slots:
    void onAudioDataReady();

private:
    void updateSpectrum(const QByteArray& chunk);
    void updateVadState(const QByteArray &chunk);
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

    // FFT 窗口大小,决定频率分辨率
    static constexpr int kFftSize = 512;      
    static constexpr int kBandCount = 16;     
    std::vector<float> m_fftInputBuffer;   
    void* m_kissFftCfg = nullptr;

    // 整段录音原始 PCM，仅用于调试导出
    QByteArray m_fullSessionBuffer;   
    bool writeWavFile(const QString& filePath, const QByteArray& pcmData,
        int sampleRate, int channels, int bitsPerSample) const;
};