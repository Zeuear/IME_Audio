#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "config/AppConfig.h"
#include "services/TextPolishService.h"
#include "services/ollama/OllamaErrors.h"
#include "services/textpolish/AiJobTracker.h"
#include "services/textpolish/AiStagePlan.h"
#include "services/textpolish/OpenAiProvider.h"
#include "services/textpolish/TextPolishPrompts.h"

using namespace TextPolishPrompts;

// ---- 思考段过滤：思考型模型的推理不能被打进前台窗口
TEST(StripThinking, RemovesThinkBlock) {
    EXPECT_EQ(stripThinking(QStringLiteral("<think>先想想\n再想想</think>\n\nHello.")), QStringLiteral("Hello."));
}

TEST(StripThinking, KeepsTextAfterDanglingCloser) {
    // 服务端吃掉了开头的 <think>，只剩结束标签
    EXPECT_EQ(stripThinking(QStringLiteral("推理过程</think>答案")), QStringLiteral("答案"));
}

TEST(StripThinking, RemovesGemmaChannelBlock) {
    EXPECT_EQ(stripThinking(QStringLiteral("<|channel>thought\n...<channel|>最终答案")), QStringLiteral("最终答案"));
}

TEST(StripThinking, PlainTextUnchanged) {
    EXPECT_EQ(stripThinking(QStringLiteral("  你好，世界。 ")), QStringLiteral("你好，世界。"));
}

// ---- 翻译专用模型识别
TEST(TranslationModel, RecognizesKnownFamilies) {
    EXPECT_TRUE(isTranslationModel(QStringLiteral("hf.co/IndexTeam/Index-Translate-9B-GGUF:Q4_K_M")));
    EXPECT_TRUE(isTranslationModel(QStringLiteral("hy-mt2:7b")));
    EXPECT_TRUE(isTranslationModel(QStringLiteral("translategemma:12b")));
    EXPECT_FALSE(isTranslationModel(QStringLiteral("gemma4:e4b-it-q4_K_M")));
    EXPECT_FALSE(isTranslationModel(QStringLiteral("qwen3:8b")));
    EXPECT_FALSE(isTranslationModel(QString()));
}

TEST(TranslationModel, PromptUsesTrainedTemplateAndLanguageName) {
    EXPECT_EQ(translationModelPrompt(QStringLiteral("英文"), QStringLiteral("你好")),
              QStringLiteral("请将以下文本翻译为英语，直接输出翻译结果，不要进行任何解释。\n\n你好"));
    EXPECT_EQ(promptLanguageName(QStringLiteral("简体中文")), QStringLiteral("简体中文"));
    EXPECT_EQ(promptLanguageName(QStringLiteral("ja")), QStringLiteral("日语"));
}

// ---- 流水线阶段：润色、翻译各自由开关决定
TEST(AiStagePlan, TranslateWorksWithoutPolish) {
    // 回归：以前只开翻译、不开润色时云端/Ollama 引擎什么都不做
    const auto plan = planAiStages(false, QStringLiteral("英文"), true, true, "qwen3:8b", {});
    EXPECT_FALSE(plan.polish);
    EXPECT_TRUE(plan.translate);
    EXPECT_FALSE(plan.combined);
    EXPECT_EQ(plan.translateModel, QStringLiteral("qwen3:8b"));
}

TEST(AiStagePlan, PolishOnlyWhenNoTarget) {
    const auto plan = planAiStages(true, QStringLiteral("不翻译"), true, true, "qwen3:8b", {});
    EXPECT_TRUE(plan.polish);
    EXPECT_FALSE(plan.translate);
}

TEST(AiStagePlan, SameGeneralModelCombinesIntoOneRequest) {
    const auto plan = planAiStages(true, QStringLiteral("英文"), true, true, "qwen3:8b", QStringLiteral("qwen3:8b"));
    EXPECT_TRUE(plan.combined);
}

TEST(AiStagePlan, DedicatedTranslatorRunsAsSecondStage) {
    const QString index = QStringLiteral("hf.co/IndexTeam/Index-Translate-9B-GGUF:Q4_K_M");
    const auto plan = planAiStages(true, QStringLiteral("英文"), true, true, "gemma4:e4b-it-q4_K_M", index);
    EXPECT_TRUE(plan.polish);
    EXPECT_TRUE(plan.translate);
    EXPECT_FALSE(plan.combined);
    EXPECT_EQ(plan.translateModel, index);
}

TEST(AiStagePlan, TranslationModelAsMainModelNeverPolishes) {
    const QString index = QStringLiteral("hf.co/IndexTeam/Index-Translate-9B-GGUF:Q4_K_M");
    const auto plan = planAiStages(true, QStringLiteral("英文"), true, true, index, {});
    EXPECT_FALSE(plan.polish);
    EXPECT_TRUE(plan.translate);
    EXPECT_EQ(plan.translateModel, index);
}

TEST(AiStagePlan, NothingWhenBothOff) {
    const auto plan = planAiStages(false, QStringLiteral("不翻译"), true, true, "qwen3:8b", {});
    EXPECT_FALSE(plan.polish);
    EXPECT_FALSE(plan.translate);
}

TEST(PolishConfigTest, SeparateTranslateModelOnlyForModelChoosingEngines) {
    PolishConfig p;
    p.model = QStringLiteral("gemini-2.5-flash");
    p.translateModel = QStringLiteral("hy-mt2:7b");
    p.aiEngineIndex = 0;   // Gemini：没有单独的翻译模型
    EXPECT_EQ(p.effectiveTranslateModel(), p.model);
    p.aiEngineIndex = 2;   // Ollama
    EXPECT_EQ(p.effectiveTranslateModel(), QStringLiteral("hy-mt2:7b"));
    p.translateModel.clear();
    EXPECT_EQ(p.effectiveTranslateModel(), p.model);
}

// ---- 请求格式
namespace {
TextPolishService::RequestParams translateParams(const QString& model) {
    TextPolishService::RequestParams p;
    p.aiEngine = 2;
    p.model = model;
    p.targetLang = QStringLiteral("英文");
    p.translateOnly = true;
    return p;
}
QJsonObject parse(const QByteArray& body) { return QJsonDocument::fromJson(body).object(); }
} // namespace

TEST(OllamaRequest, TranslationModelGetsSingleUserMessage) {
    const auto msgs = OpenAiProvider::buildMessages(QStringLiteral("你好"),
        translateParams(QStringLiteral("hf.co/IndexTeam/Index-Translate-9B-GGUF:Q4_K_M")));
    ASSERT_EQ(msgs.size(), 1);
    EXPECT_EQ(msgs[0].toObject().value("role").toString(), QStringLiteral("user"));
    EXPECT_TRUE(msgs[0].toObject().value("content").toString().endsWith(QStringLiteral("\n\n你好")));
}

TEST(OllamaRequest, GeneralModelTranslateUsesSystemRules) {
    const auto msgs = OpenAiProvider::buildMessages(QStringLiteral("你好"), translateParams(QStringLiteral("qwen3:8b")));
    ASSERT_EQ(msgs.size(), 2);
    EXPECT_EQ(msgs[0].toObject().value("role").toString(), QStringLiteral("system"));
    EXPECT_EQ(msgs[1].toObject().value("content").toString(), QStringLiteral("你好"));
}

TEST(OllamaRequest, NativeBodyDisablesThinkingAndStreaming) {
    const QJsonObject body = parse(OpenAiProvider::buildOllamaChatBody(QStringLiteral("你好"), translateParams("qwen3:8b")));
    EXPECT_EQ(body.value("model").toString(), QStringLiteral("qwen3:8b"));
    EXPECT_EQ(body.value("think").toBool(true), false);
    EXPECT_EQ(body.value("stream").toBool(true), false);
    EXPECT_FALSE(body.value("keep_alive").toString().isEmpty());
    EXPECT_DOUBLE_EQ(body.value("options").toObject().value("temperature").toDouble(-1), 0.0);
}

TEST(OllamaRequest, RetryBodyOmitsThink) {
    const QJsonObject body = parse(OpenAiProvider::buildOllamaChatBody(QStringLiteral("x"), translateParams("m"), false));
    EXPECT_FALSE(body.contains("think"));
}

TEST(OllamaRequest, PolishKeepsSomeTemperature) {
    TextPolishService::RequestParams p;
    p.aiEngine = 2;
    p.model = QStringLiteral("qwen3:8b");
    const QJsonObject body = parse(OpenAiProvider::buildOllamaChatBody(QStringLiteral("x"), p));
    EXPECT_GT(body.value("options").toObject().value("temperature").toDouble(), 0.0);
}

TEST(OllamaResponse, ExtractsContentAndStripsThinking) {
    QString err;
    const QString text = OpenAiProvider::extractOllamaChatText(
        R"({"message":{"role":"assistant","content":"<think>x</think>Hello."},"done":true})", &err);
    EXPECT_TRUE(err.isEmpty());
    EXPECT_EQ(text, QStringLiteral("Hello."));
}

TEST(OllamaResponse, ReportsErrorString) {
    QString err;
    EXPECT_TRUE(OpenAiProvider::extractOllamaChatText(R"({"error":"model 'x' not found"})", &err).isEmpty());
    EXPECT_EQ(err, QStringLiteral("model 'x' not found"));
}

TEST(OllamaResponse, OpenAiCompatibleReplyAlsoStripsThinking) {
    QString err;
    const QString text = OpenAiProvider::extractOpenAiTextFromResponse(
        R"({"choices":[{"message":{"content":"<think>a</think> Hi"}}]})", &err);
    EXPECT_EQ(text, QStringLiteral("Hi"));
}

TEST(OllamaUrl, NormalizesUserEnteredVariants) {
    EXPECT_EQ(OpenAiProvider::ollamaBaseUrl(QString()), QStringLiteral("http://localhost:11434/"));
    EXPECT_EQ(OpenAiProvider::ollamaBaseUrl(QStringLiteral("http://127.0.0.1:11434/v1/")), QStringLiteral("http://127.0.0.1:11434/"));
    EXPECT_EQ(OpenAiProvider::ollamaBaseUrl(QStringLiteral("http://h:1/v1/chat/completions")), QStringLiteral("http://h:1/"));
    EXPECT_EQ(OpenAiProvider::ollamaBaseUrl(QStringLiteral("http://h:1/api/chat")), QStringLiteral("http://h:1/"));
    // 本机 Ollama 没有 TLS：https://localhost 误填时按 http 处理；远程地址不动
    EXPECT_EQ(OpenAiProvider::ollamaBaseUrl(QStringLiteral("https://localhost:11434")), QStringLiteral("http://localhost:11434/"));
    EXPECT_EQ(OpenAiProvider::ollamaBaseUrl(QStringLiteral("https://example.com")), QStringLiteral("https://example.com/"));
}

TEST(OpenAiModels, ParsesV1ModelsIds) {
    const QStringList models = OpenAiProvider::parseOpenAiModels(R"({"object":"list","data":[{"id":"a"},{"id":"b"},{}]})");
    EXPECT_EQ(models, (QStringList{ "a", "b" }));
}

// ---- 每句的 AI 阶段状态：回调与超时兜底只有一方能收尾 ----
namespace {
AiStagePlan translateAfterPolish() {
    AiStagePlan p;
    p.polish = true; p.translate = true; p.combined = false; p.translateModel = "mt";
    return p;
}
} // namespace

TEST(AiJobTracker, BeginStartsAtTheRecognizedTextForBothSourceAndCurrent) {
    AiJobTracker t;
    t.begin(7, "hola", translateAfterPolish(), "中文");
    const AiJob* job = t.find(7);
    ASSERT_NE(job, nullptr);
    EXPECT_EQ(job->source, "hola");
    EXPECT_EQ(job->current, "hola");
    EXPECT_EQ(job->target, "中文");
    EXPECT_TRUE(t.active(7));
    EXPECT_FALSE(t.active(8));
}

TEST(AiJobTracker, AdvanceMovesTheStageInputButKeepsTheSourceForTimeoutFallbackAndUi) {
    AiJobTracker t;
    t.begin(1, "hola", translateAfterPolish(), "中文");
    ASSERT_TRUE(t.advance(1, "Hola."));         // 润色结果交给翻译阶段
    EXPECT_EQ(t.find(1)->current, "Hola.");     // 翻译阶段超时时回退到润色结果
    EXPECT_EQ(t.find(1)->source, "hola");       // 右栏要的翻译前原文不变
}

TEST(AiJobTracker, OnlyTheFirstFinisherGetsTheJobTheLateOneIsIgnored) {
    AiJobTracker t;
    t.begin(1, "hola", translateAfterPolish(), "中文");
    const auto first = t.take(1);               // 例如超时兜底先到
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->current, "hola");
    EXPECT_FALSE(t.take(1).has_value());        // 迟到的回调拿不到：忽略，不会重复结束
    EXPECT_EQ(t.find(1), nullptr);
    EXPECT_FALSE(t.advance(1, "late"));
    EXPECT_EQ(t.size(), 0);
}

TEST(AiJobTracker, JobsAreIndependentBySequence) {
    AiJobTracker t;
    t.begin(1, "a", translateAfterPolish(), "x");
    t.begin(2, "b", translateAfterPolish(), "x");
    t.take(1);
    EXPECT_TRUE(t.active(2));                   // 一句结束不影响别的句子
    EXPECT_EQ(t.find(2)->source, "b");
}

TEST(OllamaErrors, OldOllamaLoadFailureTellsTheUserToUpgrade) {
    const QString msg = explainOllamaError("unable to load model: C:\\Users\\HP\\.ollama\\models\\blobs\\sha256-7144");
    EXPECT_TRUE(msg.contains(QString::fromUtf8("升级")));
    EXPECT_TRUE(explainOllamaError("unknown model architecture: 'qwen35'").contains(QString::fromUtf8("升级")));
}

TEST(OllamaErrors, MemoryErrorSuggestsSmallerModelAndOthersPassThrough) {
    EXPECT_TRUE(explainOllamaError("model requires more system memory (9 GiB) than is available").contains(QString::fromUtf8("更小")));
    EXPECT_EQ(explainOllamaError("connection refused"), "connection refused");
}
