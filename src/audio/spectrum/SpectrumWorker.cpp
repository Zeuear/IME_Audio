#include "SpectrumWorker.h"

#include <QtMath>
#include <algorithm>
#include <cmath>

extern "C" {
#include "kiss_fft.h"
#include "kiss_fftr.h"
}

SpectrumWorker::SpectrumWorker(int sampleRate) : QObject(nullptr), m_sampleRate(sampleRate)
{
    m_kissFftCfg = kiss_fftr_alloc(kFftSize, 0, nullptr, nullptr);

    computeBandLayout();

    const float frameRateHz = static_cast<float>(m_sampleRate) / static_cast<float>(kFftSize);
    m_controller = std::make_unique<AdaptiveSpectrumController>(kBandCount, frameRateHz, m_bandCenterHz);
}

SpectrumWorker::~SpectrumWorker()
{
    if (m_kissFftCfg) {
        kiss_fftr_free(static_cast<kiss_fftr_cfg>(m_kissFftCfg));
    }
}

void SpectrumWorker::computeBandLayout()
{
    const double binHz = static_cast<double>(m_sampleRate) / kFftSize;
    const int totalBins = kFftSize / 2 + 1;

    int minBin = qMax(1, static_cast<int>(kVoiceMinHz / binHz));
    int maxBin = qMin(totalBins - 1, static_cast<int>(kVoiceMaxHz / binHz));
    if (maxBin <= minBin) maxBin = minBin + 1;

    m_bandRanges.resize(kBandCount);
    m_bandCenterHz.resize(kBandCount);

    // 频段按对数均分，低频不会挤成一团
    double logMin = std::log10(static_cast<double>(minBin));
    double logMax = std::log10(static_cast<double>(maxBin));

    for (int b = 0; b < kBandCount; ++b) {
        double lowLog = logMin + (logMax - logMin) * (static_cast<double>(b) / kBandCount);
        double highLog = logMin + (logMax - logMin) * (static_cast<double>(b + 1) / kBandCount);

        int lowBin = static_cast<int>(std::pow(10.0, lowLog));
        int highBin = static_cast<int>(std::pow(10.0, highLog));
        highBin = qMax(highBin, lowBin + 1);
        highBin = qMin(highBin, maxBin);
        lowBin = qMin(lowBin, highBin - 1);

        m_bandRanges[b] = { lowBin, highBin };

        double centerBin = (lowBin + highBin) / 2.0;
        m_bandCenterHz[b] = static_cast<float>(centerBin * binHz);
    }
}

void SpectrumWorker::processChunk(const QByteArray chunk)
{
    updateLevel(chunk);
    updateSpectrum(chunk);
}

void SpectrumWorker::updateLevel(const QByteArray& chunk)
{
    const int16_t* samples = reinterpret_cast<const int16_t*>(chunk.constData());
    const int count = chunk.size() / 2;
    if (count == 0) return;

    // 计算当前帧的瞬时 RMS
    double sumSquares = 0.0;
    for (int i = 0; i < count; ++i) {
        double v = static_cast<double>(samples[i]);
        sumSquares += v * v;
    }
    double instantRms = std::sqrt(sumSquares / count);

    // 转成 dBFS，映射到 [0,1]
    double instantDb = 20.0 * std::log10(std::max(instantRms, 1.0) / 32768.0);
    float instantNormalized = std::clamp(static_cast<float>((instantDb + 60.0) / 45.0), 0.0f, 1.0f);

    // 包络跟随：变大跟得快，变小落得慢，悬浮球不会一闪一闪
    float dtMs = (static_cast<float>(count) / m_sampleRate) * 1000.0f;
    float targetMs = (instantNormalized > m_rmsLevel) ? kLevelAttackMs : kLevelReleaseMs;
    float alpha = 1.0f - std::exp(-dtMs / targetMs);

    m_rmsLevel += alpha * (instantNormalized - m_rmsLevel);
    emit levelUpdated(0.50f * (m_rmsLevel / (m_rmsLevel + 0.50f)));
}

void SpectrumWorker::updateSpectrum(const QByteArray& chunk)
{
    const int16_t* samples = reinterpret_cast<const int16_t*>(chunk.constData());
    const int count = chunk.size() / 2;

    for (int i = 0; i < count; ++i) {
        m_fftInputBuffer.push_back(samples[i] / 32768.0f);
    }
    if (static_cast<int>(m_fftInputBuffer.size()) < kFftSize) {
        return;
    }

    // 只取最近的一个窗口，攒多了也只画最新一帧
    std::vector<float> window(m_fftInputBuffer.end() - kFftSize, m_fftInputBuffer.end());
    m_fftInputBuffer.clear();

    for (int i = 0; i < kFftSize; ++i) {
        float w = 0.5f - 0.5f * std::cos(2.0f * M_PI * i / (kFftSize - 1));
        window[i] *= w;
    }

    std::vector<kiss_fft_cpx> fftOut(kFftSize / 2 + 1);
    kiss_fftr(static_cast<kiss_fftr_cfg>(m_kissFftCfg), window.data(), fftOut.data());

    std::vector<float> magnitudes(kFftSize / 2 + 1);
    for (size_t i = 0; i < magnitudes.size(); ++i) {
        magnitudes[i] = std::sqrt(fftOut[i].r * fftOut[i].r + fftOut[i].i * fftOut[i].i);
    }

    // 每个频段取 RMS（能量平均）
    std::vector<float> bandsDb(kBandCount);
    for (int b = 0; b < kBandCount; ++b) {
        const auto& range = m_bandRanges[b];
        double sumSq = 0.0;
        int binCount = 0;
        for (int i = range.lowBin; i < range.highBin && i < static_cast<int>(magnitudes.size()); ++i) {
            sumSq += static_cast<double>(magnitudes[i]) * magnitudes[i];
            ++binCount;
        }
        float rms = (binCount > 0) ? static_cast<float>(std::sqrt(sumSq / binCount)) : 0.0f;
        bandsDb[b] = 20.0f * std::log10(std::max(rms, 1e-6f));
    }

    // 分频段自适应量程 + 心理声学权重 + 全局响度增益
    const std::vector<float>& visualBands = m_controller->process(bandsDb, m_rmsLevel);
    emit spectrumReady(QVector<float>(visualBands.begin(), visualBands.end()));
}

void SpectrumWorker::resetLevel()
{
    m_rmsLevel = 0.0f;
    emit levelUpdated(m_rmsLevel);
}
