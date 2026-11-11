#include <gtest/gtest.h>

#include <memory>

#include "config/AppConfig.h"
#include "services/TextPolishService.h"
#include "services/textpolish/TextPolishProvider.h"
#include "services/textpolish/TextPolishPrompts.h"

// TextPolishService 的能力判断和"不支持就跳过"逻辑用假 provider 覆盖，
// 不联网、不加载真实模型，避免测试依赖外部服务。
namespace {

struct FakeProviderState {
    int polishCalls = 0;
};

class FakeProvider : public TextPolishProvider {
public:
    FakeProvider(bool canPolish, bool canTranslate, bool canTest, bool canFetchModels,
                 std::shared_ptr<FakeProviderState> state)
        : m_canPolish(canPolish)
        , m_canTranslate(canTranslate)
        , m_canTest(canTest)
        , m_canFetchModels(canFetchModels)
        , m_state(std::move(state)) {}

    bool supports(const TextPolishService::RequestParams&) const override { return true; }

    bool canPolish() const override { return m_canPolish; }
    bool canTranslate() const override { return m_canTranslate; }
    bool canTest() const override { return m_canTest; }
    bool canFetchModels() const override { return m_canFetchModels; }

    void polish(const QString& inputText,
                const TextPolishService::RequestParams&,
                QNetworkAccessManager*,
                ResultCallback done) override {
        ++m_state->polishCalls;
        done(true, QStringLiteral("polished:%1").arg(inputText), {});
    }

private:
    bool m_canPolish;
    bool m_canTranslate;
    bool m_canTest;
    bool m_canFetchModels;
    std::shared_ptr<FakeProviderState> m_state;
};

class TextPolishServiceTest : public ::testing::Test {
protected:
    void useProvider(bool canPolish, bool canTranslate, bool canTest = false, bool canFetchModels = false) {
        m_state = std::make_shared<FakeProviderState>();
        m_service.setProviderFactoryForTest(
            [state = m_state, canPolish, canTranslate, canTest, canFetchModels]
            (const TextPolishService::RequestParams&) {
                return std::make_unique<FakeProvider>(canPolish, canTranslate, canTest, canFetchModels, state);
            });
    }

    TextPolishService m_service{ nullptr };
    std::shared_ptr<FakeProviderState> m_state;
};

} // namespace

TEST_F(TextPolishServiceTest, PolishingEngineIsCalledAndResultForwarded) {
    useProvider(true, true);

    TextPolishService::RequestParams params;
    params.targetLang = QStringLiteral("不翻译");

    bool called = false;
    bool ok = false;
    QString text;
    QString error;
    m_service.polishText(QStringLiteral("hello"), params,
                         [&](bool s, const QString& t, const QString& e) {
                             called = true;
                             ok = s;
                             text = t;
                             error = e;
                         });

    EXPECT_TRUE(called);
    EXPECT_TRUE(ok);
    EXPECT_EQ(text, QStringLiteral("polished:hello"));
    EXPECT_TRUE(error.isEmpty());
    EXPECT_EQ(m_state->polishCalls, 1);
}

TEST_F(TextPolishServiceTest, EngineWithoutPolishOrTranslateSkipsSynchronously) {
    useProvider(false, false);

    TextPolishService::RequestParams params;
    params.targetLang = QStringLiteral("英文");

    bool called = false;
    bool ok = false;
    QString text;
    m_service.polishText(QStringLiteral("原文"), params,
                         [&](bool s, const QString& t, const QString&) {
                             called = true;
                             ok = s;
                             text = t;
                         });

    // 跳过必须同步完成，调用方的排队逻辑依赖这个顺序
    EXPECT_TRUE(called);
    EXPECT_TRUE(ok);
    EXPECT_EQ(text, QStringLiteral("原文"));
    EXPECT_EQ(m_state->polishCalls, 0);
}

TEST_F(TextPolishServiceTest, TranslationOnlyEngineRunsWhenTargetLanguageSet) {
    useProvider(false, true);

    TextPolishService::RequestParams params;
    params.targetLang = QStringLiteral("英文");

    bool called = false;
    m_service.polishText(QStringLiteral("原文"), params,
                         [&](bool, const QString&, const QString&) { called = true; });

    EXPECT_TRUE(called);
    EXPECT_EQ(m_state->polishCalls, 1);
}

TEST_F(TextPolishServiceTest, TranslationOnlyEngineSkipsWhenNoTargetLanguage) {
    useProvider(false, true);

    for (const QString& target : { QString(), QStringLiteral("不翻译") }) {
        m_state->polishCalls = 0;
        TextPolishService::RequestParams params;
        params.targetLang = target;

        bool called = false;
        QString text;
        m_service.polishText(QStringLiteral("原文"), params,
                             [&](bool, const QString& t, const QString&) {
                                 called = true;
                                 text = t;
                             });

        EXPECT_TRUE(called);
        EXPECT_EQ(text, QStringLiteral("原文"));
        EXPECT_EQ(m_state->polishCalls, 0);
    }
}

// 只翻译的请求（文件转录的译文栏）不能按"原文即结果"放行，否则译文栏里显示的是原文
TEST_F(TextPolishServiceTest, TranslateOnlyFailsInsteadOfReturningOriginal) {
    struct Case { bool canTranslate; QString target; };
    for (const Case& c : { Case{ false, QStringLiteral("英文") }, Case{ true, QString() },
                           Case{ true, QStringLiteral("不翻译") } }) {
        useProvider(true, c.canTranslate);
        TextPolishService::RequestParams params;
        params.targetLang = c.target;
        params.translateOnly = true;

        bool called = false;
        bool ok = true;
        QString text;
        m_service.polishText(QStringLiteral("原文"), params,
                             [&](bool s, const QString& t, const QString&) {
                                 called = true;
                                 ok = s;
                                 text = t;
                             });

        EXPECT_TRUE(called);
        EXPECT_FALSE(ok);
        EXPECT_TRUE(text.isEmpty());
        EXPECT_EQ(m_state->polishCalls, 0);
    }
}

TEST_F(TextPolishServiceTest, TranslateOnlyRunsWhenEngineCanTranslate) {
    useProvider(false, true);

    TextPolishService::RequestParams params;
    params.targetLang = QStringLiteral("英文");
    params.translateOnly = true;

    bool ok = false;
    m_service.polishText(QStringLiteral("原文"), params,
                         [&](bool s, const QString&, const QString&) { ok = s; });

    EXPECT_TRUE(ok);
    EXPECT_EQ(m_state->polishCalls, 1);
}

TEST(TextPolishPromptsTest, TranslateOnlyPromptNamesTargetLanguage) {
    EXPECT_TRUE(TextPolishPrompts::translateOnlyPrompt(QStringLiteral("英文")).contains(QStringLiteral("「英文」")));
}

TEST_F(TextPolishServiceTest, CapabilitiesComeFromProvider) {
    useProvider(false, true, true, false);

    const auto caps = m_service.capabilities(2);
    EXPECT_FALSE(caps.canPolish);
    EXPECT_TRUE(caps.canTranslate);
    EXPECT_TRUE(caps.canTest);
    EXPECT_FALSE(caps.canFetchModels);
}

TEST(TextPolishPromptsTest, HasTranslationTarget) {
    EXPECT_FALSE(TextPolishPrompts::hasTranslationTarget({}));
    EXPECT_FALSE(TextPolishPrompts::hasTranslationTarget(QStringLiteral("不翻译")));
    EXPECT_TRUE(TextPolishPrompts::hasTranslationTarget(QStringLiteral("英文")));
}

// 界面下拉存的是简体键，之前提示词只认繁体，导致这几项选了也不生效
TEST(TextPolishPromptsTest, UiStyleKeysSelectTheirInstructions) {
    auto partsFor = [](const QString& style) {
        TextPolishService::RequestParams params;
        params.style = style;
        params.customPrompt = QStringLiteral("请加上感叹号");
        return TextPolishPrompts::buildPolishInstructionParts(params);
    };

    const auto defaults = partsFor(QStringLiteral("智慧预设"));
    EXPECT_NE(partsFor(QStringLiteral("商务正式")).style, defaults.style);
    EXPECT_NE(partsFor(QStringLiteral("简洁扼要")).style, defaults.style);
    EXPECT_EQ(partsFor(QStringLiteral("商務正式")).style, partsFor(QStringLiteral("商务正式")).style);

    EXPECT_TRUE(defaults.customInstruction.isEmpty());
    EXPECT_FALSE(partsFor(QStringLiteral("自订Prompt")).customInstruction.isEmpty());
}

// 本地离线翻译是引擎下拉里的一项：不润色、只翻译，没有连接测试和模型列表，也不能走云端 provider
TEST_F(TextPolishServiceTest, LocalTranslationEngineHasNoCloudCapabilities) {
    const auto caps = m_service.capabilities(kLocalTranslationEngine);
    EXPECT_FALSE(caps.canPolish);
    EXPECT_TRUE(caps.canTranslate);
    EXPECT_FALSE(caps.canTest);
    EXPECT_FALSE(caps.canFetchModels);
}

TEST_F(TextPolishServiceTest, LocalTranslationEngineNeverCallsCloudProvider) {
    useProvider(true, true);

    TextPolishService::RequestParams params;
    params.aiEngine = kLocalTranslationEngine;
    params.targetLang = QStringLiteral("英文");

    QString text;
    bool called = false;
    m_service.polishText(QStringLiteral("原文"), params,
                         [&](bool, const QString& t, const QString&) { called = true; text = t; });

    EXPECT_TRUE(called);
    EXPECT_EQ(text, QStringLiteral("原文"));
    EXPECT_EQ(m_state->polishCalls, 0);
}
