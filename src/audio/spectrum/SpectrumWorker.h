#pragma once
#include <QByteArray>
#include <QObject>
#include <QVector>
#include <memory>
#include <vector>

#include "AdaptiveSpectrumController.h"

// @brief 悬浮球可视化的数据源：从录音块里算出整体音量和 16 段频谱。
// 运行在独立线程，只做显示用途，不影响录音和识别。
class SpectrumWorker : public QObject {
    Q_OBJECT
public:
    explicit SpectrumWorker(int sampleRate);
    ~SpectrumWorker() override;

public slots:
    void processChunk(const QByteArray chunk);
    // 停止/暂停录音时把音量归零，悬浮球不会停在最后一帧的高度
    void resetLevel();

signals:
    void spectrumReady(const QVector<float>& bands);
    void levelUpdated(float rmsLevel);

private:
    struct BandRange {
        int lowBin;
        int highBin;
    };
    void computeBandLayout();
    void updateLevel(const QByteArray& chunk);
    void updateSpectrum(const QByteArray& chunk);

    static constexpr int kFftSize = 512;
    static constexpr int kBandCount = 16;
    // 语音可懂度核心区覆盖到 3000~4000Hz
    static constexpr double kVoiceMinHz = 400.0;
    static constexpr double kVoiceMaxHz = 4800.0;
    static constexpr float kLevelAttackMs = 15.0f;
    static constexpr float kLevelReleaseMs = 50.0f;

    int m_sampleRate;
    std::unique_ptr<AdaptiveSpectrumController> m_controller;
    void* m_kissFftCfg = nullptr;
    std::vector<float> m_fftInputBuffer;

    // 预计算的频段布局
    std::vector<BandRange> m_bandRanges;
    std::vector<float> m_bandCenterHz;

    float m_rmsLevel = 0.0f;
};
