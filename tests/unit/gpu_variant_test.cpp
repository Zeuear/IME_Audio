#include <gtest/gtest.h>
#include "sherpa/GpuVariant.h"

using namespace GpuVariant;

namespace {
QList<Link> sampleTable()
{
    return {
        { "org/cpu-a", "me/gpu-a", false },
        { "me/gpu-a", "", true },
        { "me/gpu-only", "", true },   // 没有 CPU 版（如 1.7B）
        { "org/plain", "", false },
    };
}
}

TEST(GpuVariantTest, ShouldUseCudaFourCombinations)
{
    EXPECT_TRUE(shouldUseCuda(true, true));
    EXPECT_FALSE(shouldUseCuda(true, false));
    EXPECT_FALSE(shouldUseCuda(false, true));
    EXPECT_FALSE(shouldUseCuda(false, false));
}

TEST(GpuVariantTest, GpuAndCpuVariantLookupAreInverse)
{
    const auto t = sampleTable();
    EXPECT_EQ(gpuVariantOf(t, "org/cpu-a"), QString("me/gpu-a"));
    EXPECT_EQ(cpuVariantOf(t, "me/gpu-a"), QString("org/cpu-a"));
}

TEST(GpuVariantTest, NoPairingReturnsEmpty)
{
    const auto t = sampleTable();
    EXPECT_TRUE(gpuVariantOf(t, "org/plain").isEmpty());
    EXPECT_TRUE(cpuVariantOf(t, "me/gpu-only").isEmpty());
}

TEST(GpuVariantTest, UnknownRepoReturnsEmpty)
{
    const auto t = sampleTable();
    EXPECT_TRUE(gpuVariantOf(t, "nope/none").isEmpty());
    EXPECT_TRUE(cpuVariantOf(t, "nope/none").isEmpty());
    EXPECT_TRUE(cpuVariantOf(t, "").isEmpty());
    EXPECT_FALSE(isGpuVariant(t, "nope/none"));
}

TEST(GpuVariantTest, IsGpuVariantFollowsFlag)
{
    const auto t = sampleTable();
    EXPECT_TRUE(isGpuVariant(t, "me/gpu-a"));
    EXPECT_FALSE(isGpuVariant(t, "org/cpu-a"));
}
