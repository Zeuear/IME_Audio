#include <gtest/gtest.h>

#include "TextPolishService.h"
#include "../../src/textpolish//OpenAiProvider.h"

// Ollama 模型列表解析（纯函数，无网络依赖）
TEST(OllamaTest, ParseTagsExtractsModelNames) {
    QByteArray json = R"({
        "models": [
            {"name": "qwen2.5:7b", "size": 123},
            {"name": "llama3:8b", "size": 456},
            {"name": "nomic-embed-text", "size": 1}
        ]
    })";
    QStringList models = OpenAiProvider::parseOllamaTags(json);
    ASSERT_EQ(models.size(), 3);
    EXPECT_EQ(models[0], "qwen2.5:7b");
    EXPECT_EQ(models[1], "llama3:8b");
    EXPECT_EQ(models[2], "nomic-embed-text");
}

TEST(OllamaTest, ParseTagsEmptyOnMalformedJson) {
    EXPECT_TRUE(OpenAiProvider::parseOllamaTags("not json{").isEmpty());
}

TEST(OllamaTest, ParseTagsSkipsEntriesWithoutName) {
    QByteArray json = R"({"models": [{"size": 1}, {"name": "qwen2.5:7b"}]})";
    QStringList models = OpenAiProvider::parseOllamaTags(json);
    ASSERT_EQ(models.size(), 1);
    EXPECT_EQ(models[0], "qwen2.5:7b");
}

TEST(OllamaTest, ParseTagsHandlesEmptyModelsArray) {
    QByteArray json = R"({"models": []})";
    EXPECT_TRUE(OpenAiProvider::parseOllamaTags(json).isEmpty());
}
