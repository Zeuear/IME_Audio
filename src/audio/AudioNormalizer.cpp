#include "AudioNormalizer.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

QByteArray AudioNormalizer::normalizePcm16(const QByteArray& pcm, double* gainOut)
{
    if (gainOut) *gainOut = 1.0;
    const qsizetype count = pcm.size() / 2;
    if (count == 0) return pcm;

    const int16_t* in = reinterpret_cast<const int16_t*>(pcm.constData());
    int peak = 0;
    double sumSquares = 0.0;
    for (qsizetype i = 0; i < count; ++i) {
        peak = std::max(peak, std::abs(static_cast<int>(in[i])));
        sumSquares += static_cast<double>(in[i]) * in[i];
    }
    const double rms = std::sqrt(sumSquares / static_cast<double>(count));
    if (peak == 0 || rms < kNoiseFloorRms) return pcm;

    const double gain = std::min(kTargetPeak * 32768.0 / peak, kMaxGain);
    // 增益不足 5% 听不出差别，省掉一次拷贝
    if (gain < 1.05) return pcm;

    QByteArray out(pcm.size(), Qt::Uninitialized);
    int16_t* dst = reinterpret_cast<int16_t*>(out.data());
    for (qsizetype i = 0; i < count; ++i) {
        const double v = std::round(in[i] * gain);
        dst[i] = static_cast<int16_t>(std::clamp(v, -32768.0, 32767.0));
    }
    // 奇数字节的尾巴原样保留，保持长度不变
    if (pcm.size() % 2) out[pcm.size() - 1] = pcm[pcm.size() - 1];

    if (gainOut) *gainOut = gain;
    return out;
}
