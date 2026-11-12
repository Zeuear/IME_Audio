#include <gtest/gtest.h>

#include <cmath>
#include <random>
#include <vector>

#include "audio/separation/MdxDsp.h"

namespace {
constexpr int kNFft = 7680;
constexpr int kHop = 1024;
constexpr int kChunk = kHop * 255;   // UVR-MDX-NET-Voc_FT 一块的样本数
}

TEST(MdxDsp, FrameCountMatchesModelInput)
{
    MdxDsp::Stft stft(kNFft, kHop);
    EXPECT_EQ(stft.frameCount(kChunk), 256);   // 模型输入的 dim_t
    EXPECT_EQ(stft.bins(), 3841);
}

TEST(MdxDsp, InverseRestoresTheSignal)
{
    std::mt19937 rng(1);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::vector<float> x(kChunk);
    for (float& v : x) v = dist(rng);

    MdxDsp::Stft stft(kNFft, kHop);
    std::vector<std::complex<float>> spec;
    stft.forward(x.data(), kChunk, spec);
    std::vector<float> y(kChunk);
    stft.inverse(spec.data(), stft.frameCount(kChunk), kChunk, y.data());

    // 两端的窗口覆盖不足，只比较中间（分离时两端本来就会被丢弃）
    double maxErr = 0;
    for (int i = kNFft / 2; i < kChunk - kNFft / 2; ++i)
        maxErr = std::max(maxErr, double(std::fabs(x[i] - y[i])));
    EXPECT_LT(maxErr, 1e-3);
}

TEST(MdxDsp, SineEnergyLandsInExpectedBin)
{
    // 4410 Hz @ 44.1 kHz：频点间隔 44100/7680，预期落在第 768 个频点
    std::vector<float> x(kChunk);
    for (int i = 0; i < kChunk; ++i) x[i] = std::sin(2.0f * 3.14159265f * 4410.0f * i / 44100.0f);

    MdxDsp::Stft stft(kNFft, kHop);
    std::vector<std::complex<float>> spec;
    stft.forward(x.data(), kChunk, spec);

    const std::complex<float>* mid = spec.data() + size_t(128) * stft.bins();
    int peak = 0;
    for (int k = 1; k < stft.bins(); ++k)
        if (std::abs(mid[k]) > std::abs(mid[peak])) peak = k;
    EXPECT_EQ(peak, 768);
}
