#include <gtest/gtest.h>

#include "sherpa/SherpaPunctuator.h"
#include "sherpa/SherpaConfig.h"

// ---- SherpaPunctuator 契约（无需真实 onnx 模型） ----

TEST(SherpaPunctuatorTest, NotLoadedReturnsInputUnchanged) {
    SherpaPunctuator p;
    EXPECT_FALSE(p.isLoaded());
    // 未加载时直接透传，调用方回退启发式
    EXPECT_EQ(p.punctuate("你好世界"), QString("你好世界"));
    EXPECT_EQ(p.punctuate(""), QString());
}

TEST(SherpaPunctuatorTest, LoadMissingModelFileFails) {
    SherpaPunctuator p;
    // 不存在的目录 → 加载失败，保持未加载
    EXPECT_FALSE(p.load("C:/no/such/punct/dir"));
    EXPECT_FALSE(p.isLoaded());
}

TEST(SherpaPunctuatorTest, UnloadResetsState) {
    SherpaPunctuator p;
    p.unload();
    EXPECT_FALSE(p.isLoaded());
    EXPECT_EQ(p.punctuate("abc"), QString("abc"));
}

// ---- ModelRegistry::shouldUseNeuralPunct 判定逻辑 ----

namespace {
ModelDescriptor makeDesc(const QString& lang, bool builtin, PunctMode mode) {
    ModelDescriptor d(ModelArch::Paraformer);
    d.language = lang;
    d.hasBuiltinPunctuation = builtin;
    d.punctMode = mode;
    return d;
}
}

TEST(NeuralPunctConfigTest, ZhEnAutoBinds) {
    EXPECT_TRUE(ModelRegistry::shouldUseNeuralPunct(makeDesc("Chinese", false, PunctMode::Auto)));
    EXPECT_TRUE(ModelRegistry::shouldUseNeuralPunct(makeDesc("English", false, PunctMode::Auto)));
}

TEST(NeuralPunctConfigTest, OffModeNeverBinds) {
    EXPECT_FALSE(ModelRegistry::shouldUseNeuralPunct(makeDesc("Chinese", false, PunctMode::Off)));
    EXPECT_FALSE(ModelRegistry::shouldUseNeuralPunct(makeDesc("English", false, PunctMode::Off)));
}

TEST(NeuralPunctConfigTest, BuiltinPunctuationSkips) {
    // SenseVoice / Canary 自带标点 → 不绑神经 punc
    EXPECT_FALSE(ModelRegistry::shouldUseNeuralPunct(makeDesc("Chinese", true, PunctMode::Auto)));
    EXPECT_FALSE(ModelRegistry::shouldUseNeuralPunct(makeDesc("English", true, PunctMode::Auto)));
}

TEST(NeuralPunctConfigTest, NonZhEnSkips) {
    // 日/韩/法等非中英 → 跳过，避免错插中文标点
    EXPECT_FALSE(ModelRegistry::shouldUseNeuralPunct(makeDesc("Japanese", false, PunctMode::Auto)));
    EXPECT_FALSE(ModelRegistry::shouldUseNeuralPunct(makeDesc("Korean", false, PunctMode::Auto)));
    EXPECT_FALSE(ModelRegistry::shouldUseNeuralPunct(makeDesc("French", false, PunctMode::Auto)));
}

TEST(NeuralPunctConfigTest, SharedDirIsDeterministic) {
    // 共享目录稳定，不随调用变化
    EXPECT_EQ(ModelRegistry::NeuralPunctModel::sharedDir(),
              ModelRegistry::NeuralPunctModel::sharedDir());
    EXPECT_TRUE(ModelRegistry::NeuralPunctModel::sharedDir().endsWith("punct-zh-en"));
}
