#include <gtest/gtest.h>

#include "textpolish/OpenAiProvider.h"

// OpenAiProvider::test 的 OpenAI 兼容路径用 isValidModelsResponse 判定
// /v1/models 响应是否合法。这里做无网络的纯函数测试。

TEST(OpenAiProviderTest, IsValidModelsResponse_AcceptsOpenAiList) {
    QByteArray json = R"({"object":"list","data":[{"id":"gpt-3.5-turbo"},{"id":"gpt-4"}]})";
    EXPECT_TRUE(OpenAiProvider::isValidModelsResponse(json));
}

TEST(OpenAiProviderTest, IsValidModelsResponse_RejectsEmpty) {
    EXPECT_FALSE(OpenAiProvider::isValidModelsResponse(QByteArray()));
}

TEST(OpenAiProviderTest, IsValidModelsResponse_RejectsGarbage) {
    EXPECT_FALSE(OpenAiProvider::isValidModelsResponse("not json at all"));
}

TEST(OpenAiProviderTest, IsValidModelsResponse_RejectsWrongShape) {
    // 没有 "object":"list" / "data" 结构（例如返回的是 chat 响应）
    QByteArray json = R"({"choices":[{"message":{"content":"hi"}}]})";
    EXPECT_FALSE(OpenAiProvider::isValidModelsResponse(json));
}
