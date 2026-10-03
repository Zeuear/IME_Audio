#include "AudioFormatConverter.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

float decodeSample(const char* p, QAudioFormat::SampleFormat format)
{
    switch (format) {
    case QAudioFormat::UInt8:
        return (static_cast<uint8_t>(*p) - 128) / 128.0f;
    case QAudioFormat::Int16: {
        int16_t v; memcpy(&v, p, sizeof(v));
        return v / 32768.0f;
    }
    case QAudioFormat::Int32: {
        int32_t v; memcpy(&v, p, sizeof(v));
        return v / 2147483648.0f;
    }
    case QAudioFormat::Float: {
        float v; memcpy(&v, p, sizeof(v));
        return v;
    }
    default:
        return 0.0f;
    }
}

} // namespace

namespace AudioFormatConverter {

bool isInternalFormat(const QAudioFormat& format, int targetSampleRate)
{
    return format.sampleFormat() == QAudioFormat::Int16
        && format.channelCount() == 1
        && format.sampleRate() == targetSampleRate;
}

QByteArray toInternalPcm(const QByteArray& chunk, const QAudioFormat& sourceFormat, int targetSampleRate)
{
    const int channels = std::max(1, sourceFormat.channelCount());
    const int bytesPerSample = sourceFormat.bytesPerSample();
    if (bytesPerSample <= 0) return {};

    const qsizetype frames = chunk.size() / (bytesPerSample * channels);
    if (frames == 0) return {};

    // 1) 解码并混成单声道 float
    std::vector<float> mono(static_cast<size_t>(frames));
    const char* raw = chunk.constData();
    for (qsizetype i = 0; i < frames; ++i) {
        float acc = 0.0f;
        for (int c = 0; c < channels; ++c) {
            acc += decodeSample(raw + (i * channels + c) * bytesPerSample, sourceFormat.sampleFormat());
        }
        mono[static_cast<size_t>(i)] = acc / channels;
    }

    // 2) 重采样到目标采样率。降采样时对窗口内取平均，相当于一个最简单的低通，
    //    避免高频折叠成噪声；升采样（少见）用线性插值。
    const double step = static_cast<double>(sourceFormat.sampleRate()) / targetSampleRate;
    const qsizetype outCount = static_cast<qsizetype>(frames / step);
    QByteArray out(static_cast<qsizetype>(outCount * sizeof(int16_t)), Qt::Uninitialized);
    int16_t* dst = reinterpret_cast<int16_t*>(out.data());

    for (qsizetype k = 0; k < outCount; ++k) {
        float v;
        if (step > 1.0) {
            const qsizetype begin = static_cast<qsizetype>(k * step);
            const qsizetype end = std::min<qsizetype>(frames, std::max<qsizetype>(begin + 1, static_cast<qsizetype>((k + 1) * step)));
            float sum = 0.0f;
            for (qsizetype i = begin; i < end; ++i) sum += mono[static_cast<size_t>(i)];
            v = sum / static_cast<float>(end - begin);
        } else {
            const double pos = k * step;
            const qsizetype i0 = static_cast<qsizetype>(pos);
            const qsizetype i1 = std::min<qsizetype>(i0 + 1, frames - 1);
            const float frac = static_cast<float>(pos - i0);
            v = mono[static_cast<size_t>(i0)] * (1.0f - frac) + mono[static_cast<size_t>(i1)] * frac;
        }
        // 3) 转回 Int16
        dst[k] = static_cast<int16_t>(std::clamp(v, -1.0f, 1.0f) * 32767.0f);
    }
    return out;
}

}
