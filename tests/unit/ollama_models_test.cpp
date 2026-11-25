// Ollama 模型下载：/api/pull 流式进度解析、模型名比较、推荐模型表、下拉框旁下载按钮的显示状态
#include <gtest/gtest.h>

#include "services/ollama/OllamaCatalog.h"
#include "services/ollama/OllamaClient.h"
#include "ui/controls/OllamaModelButton.h"

TEST(OllamaPullStream, ParsesCompleteLinesAndKeepsPartialTail) {
    QByteArray buf = R"({"status":"pulling manifest"}
{"status":"pulling abc","digest":"sha256:abc","total":2000,"completed":500}
{"status":"pull)";
    const auto events = OllamaClient::takePullEvents(buf);
    ASSERT_EQ(events.size(), 2);
    EXPECT_EQ(events[0].status, QStringLiteral("pulling manifest"));
    EXPECT_EQ(events[0].total, -1);
    EXPECT_EQ(events[1].total, 2000);
    EXPECT_EQ(events[1].completed, 500);
    // 半行留在缓冲区，等下一块数据拼完整
    EXPECT_EQ(buf, QByteArray(R"({"status":"pull)"));
}

TEST(OllamaPullStream, RecognizesSuccessAndError) {
    QByteArray buf = "{\"status\":\"success\"}\n{\"error\":\"pull model manifest: file does not exist\"}\n";
    const auto events = OllamaClient::takePullEvents(buf);
    ASSERT_EQ(events.size(), 2);
    EXPECT_TRUE(events[0].success);
    EXPECT_FALSE(events[1].success);
    EXPECT_EQ(events[1].error, QStringLiteral("pull model manifest: file does not exist"));
    EXPECT_TRUE(buf.isEmpty());
}

TEST(OllamaModelName, ImplicitLatestTagAndCaseInsensitive) {
    EXPECT_TRUE(OllamaClient::sameModel("qwen3", "qwen3:latest"));
    EXPECT_TRUE(OllamaClient::sameModel("Qwen3:8B", "qwen3:8b"));
    EXPECT_FALSE(OllamaClient::sameModel("qwen3:8b", "qwen3:4b"));
    // hf.co 路径里的斜杠不能被当成标签分隔
    EXPECT_TRUE(OllamaClient::sameModel("hf.co/IndexTeam/Index-Translate-9B-GGUF",
                                        "hf.co/indexteam/index-translate-9b-gguf:latest"));
}

TEST(OllamaCatalog, SplitsRecommendationsByPurpose) {
    const QStringList translate = ollamaCatalogNames(OllamaPurpose::Translate);
    const QStringList polish = ollamaCatalogNames(OllamaPurpose::Polish);
    EXPECT_TRUE(translate.contains("hf.co/IndexTeam/Index-Translate-9B-GGUF:Q4_K_M"));
    EXPECT_FALSE(polish.contains("hf.co/IndexTeam/Index-Translate-9B-GGUF:Q4_K_M"));
    EXPECT_FALSE(polish.isEmpty());
    EXPECT_EQ(ollamaCatalogSize("hf.co/IndexTeam/Index-Translate-9B-GGUF:Q4_K_M"), QStringLiteral("5.8 GB"));
    EXPECT_TRUE(translate.contains("hf.co/IndexTeam/Index-Translate-9B-GGUF:Q3_K_M"));
    EXPECT_EQ(ollamaCatalogSize("hf.co/IndexTeam/Index-Translate-9B-GGUF:Q3_K_M"), QStringLiteral("4.7 GB"));
    EXPECT_TRUE(translate.contains("hf.co/IndexTeam/Index-Translate-2B-GGUF:Q4_K_M"));
    EXPECT_EQ(ollamaCatalogSize("hf.co/IndexTeam/Index-Translate-2B-GGUF:Q4_K_M"), QStringLiteral("1.3 GB"));
}

// 按钮只在 Ollama 引擎、服务已检测、所选模型没装时出现
TEST(OllamaModelButtonState, HiddenUnlessActiveAndModelMissing) {
    OllamaModelButton btn;
    btn.setModel("hf.co/IndexTeam/Index-Translate-9B-GGUF:Q4_K_M");
    EXPECT_TRUE(btn.isHidden());   // 不是 Ollama 引擎

    btn.setActive(true);
    EXPECT_TRUE(btn.isHidden());   // 还没检测到服务

    btn.setInstalled({ "qwen3:8b" });
    EXPECT_FALSE(btn.isHidden());  // 所选模型没装：显示下载
    EXPECT_FALSE(btn.text().isEmpty());

    btn.setModel("qwen3:8b");
    EXPECT_TRUE(btn.isHidden());   // 已装

    btn.setModel({});
    EXPECT_TRUE(btn.isHidden());   // "跟随润色模型"

    btn.setActive(false);
    btn.setModel("not-installed:1b");
    EXPECT_TRUE(btn.isHidden());
}
