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

TEST(GpuVariantTest, FileListWithoutDataFile)
{
    const auto l = onnxFileList("conv.onnx", "encoder.onnx", "decoder.onnx", "", "tokenizer", {"merges.txt", "vocab.json"});
    ASSERT_EQ(l.size(), 5);
    EXPECT_EQ(l[2], qMakePair(QString(), QString("decoder.onnx")));
    EXPECT_EQ(l[3], qMakePair(QString("tokenizer"), QString("merges.txt")));
}

TEST(GpuVariantTest, FileListDataFileSharesDirectoryWithOnnx)
{
    const auto l = onnxFileList("conv.onnx", "encoder.onnx", "decoder.onnx", "decoder.onnx.data", "tokenizer", {});
    ASSERT_EQ(l.size(), 4);
    EXPECT_EQ(l[3], qMakePair(QString(), QString("decoder.onnx.data")));
    EXPECT_EQ(l[2].first, l[3].first);
}

TEST(GpuVariantTest, VisibleInModeSplitsRowsWithoutOverlap)
{
    // 一组行在两种模式下可见的数量之和 = 总数，且互不重叠
    const bool flags[] = { false, true, false, false, true };
    int cpuVisible = 0, gpuVisible = 0;
    for (bool v : flags) {
        EXPECT_NE(visibleInMode(v, false), visibleInMode(v, true));
        cpuVisible += visibleInMode(v, false);
        gpuVisible += visibleInMode(v, true);
    }
    EXPECT_EQ(cpuVisible, 3);
    EXPECT_EQ(gpuVisible, 2);
    EXPECT_EQ(cpuVisible + gpuVisible, 5);
}
