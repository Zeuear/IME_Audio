#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <vector>

#include "audio/AudioNormalizer.h"

namespace {

QByteArray toPcm(const std::vector<int16_t>& samples)
{
    return QByteArray(reinterpret_cast<const char*>(samples.data()),
                      static_cast<qsizetype>(samples.size() * sizeof(int16_t)));
}

int peakOf(const QByteArray& pcm)
{
    const int16_t* s = reinterpret_cast<const int16_t*>(pcm.constData());
    int peak = 0;
    for (qsizetype i = 0; i < pcm.size() / 2; ++i) peak = std::max(peak, std::abs(static_cast<int>(s[i])));
    return peak;
}

// 幅度为 amp 的正弦波，RMS 约 amp * 0.707
QByteArray sine(int amp, int count = 16000)
{
    std::vector<int16_t> v(count);
    for (int i = 0; i < count; ++i)
        v[i] = static_cast<int16_t>(amp * std::sin(2.0 * 3.14159265358979 * 440.0 * i / 16000.0));
    return toPcm(v);
}

}

TEST(AudioNormalizerTest, QuietSpeechIsAmplifiedToTargetPeak)
{
    double gain = 0;
    const QByteArray out = AudioNormalizer::normalizePcm16(sine(1500), &gain);
    EXPECT_NEAR(peakOf(out), 0.7 * 32768, 400);
    EXPECT_GT(gain, 10.0);
    EXPECT_EQ(out.size(), 32000);
}

TEST(AudioNormalizerTest, AlreadyLoudAudioIsUntouched)
{
    const QByteArray in = sine(24000);
    double gain = 0;
    EXPECT_EQ(AudioNormalizer::normalizePcm16(in, &gain), in);
    EXPECT_DOUBLE_EQ(gain, 1.0);
}

TEST(AudioNormalizerTest, LoudAudioIsNeverAttenuated)
{
    const QByteArray in = sine(32000);
    EXPECT_EQ(AudioNormalizer::normalizePcm16(in), in);
}

TEST(AudioNormalizerTest, SilenceAndNoiseFloorAreNotAmplified)
{
    const QByteArray silence = toPcm(std::vector<int16_t>(16000, 0));
    EXPECT_EQ(AudioNormalizer::normalizePcm16(silence), silence);

    // RMS 约 21，低于底噪门限
    const QByteArray hiss = sine(30);
    double gain = 0;
    EXPECT_EQ(AudioNormalizer::normalizePcm16(hiss, &gain), hiss);
    EXPECT_DOUBLE_EQ(gain, 1.0);
}

TEST(AudioNormalizerTest, GainIsCapped)
{
    // 峰值 200、RMS 约 141：不低于门限，但理想增益 114 倍，必须被限到 20 倍
    double gain = 0;
    const QByteArray out = AudioNormalizer::normalizePcm16(sine(200), &gain);
    EXPECT_DOUBLE_EQ(gain, AudioNormalizer::kMaxGain);
    EXPECT_NEAR(peakOf(out), 200 * 20, 40);
}

TEST(AudioNormalizerTest, NeverClips)
{
    std::vector<int16_t> v(16000, 1000);
    v[100] = -32768;   // 极端值放在输入里，输出也不能溢出
    const QByteArray out = AudioNormalizer::normalizePcm16(toPcm(v));
    EXPECT_LE(peakOf(out), 32768);
}

TEST(AudioNormalizerTest, EmptyInputReturnsEmpty)
{
    double gain = 0;
    EXPECT_TRUE(AudioNormalizer::normalizePcm16(QByteArray(), &gain).isEmpty());
    EXPECT_DOUBLE_EQ(gain, 1.0);
}
