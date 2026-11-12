#include <gtest/gtest.h>

#include <QHash>
#include <QObject>
#include <QStringList>

#include "config/AppConfig.h"
#include "services/TextPolishService.h"
#include "services/TextPostProcessor.h"
#include "services/WorkflowManager.h"
#include "services/interfaces/workflow_interfaces.h"
#include "services/textpolish/TextPolishProvider.h"
#include "utils/EnhancementFallback.h"

// 主链路集成测试：录音(utteranceReady) → 转录 → 术语替换/标点 → 润色 → WorkflowManager 输出。
// 录音与 ASR 用假实现（脱离 WASAPI / sherpa），后处理、润色服务、回退决策、状态机都是真实代码，
// 用来拦截"各段单测都过、拼起来却错位"这一类回归。
namespace {

class PipelineRecorder : public IRecorder {
public:
    bool startListening() override { listening = true; return true; }
    void stopListening() override { listening = false; }
    bool isListening() const override { return listening; }
    bool isPaused() const override { return false; }
    void pause() override {}
    void resume() override {}
    void updateConfig() override {}
    void playTestTone() override {}

    // pcm 内容即"说的话"的标识，假 ASR 据此查表
    void speak(const QByteArray& pcm) { emit utteranceReady(pcm, 16000); }
    void fail(const QString& title, const QString& cause) { emit errorOccurred(title, cause); }

    bool listening = false;
};

class PipelineModel : public ISherpaModel {
public:
    bool reloadModel(const AppConfig&) override { return false; }   // 已加载，直接进入录音
    void loadModelAsync(const AppConfig&, bool) override {}
    bool isModelLoaded() const override { return true; }
    void pauseIdleTimer() override {}
    void resumeIdleTimer() override {}
};

// 与 TranscriptionService::onSherpaTranscribed / finish 同序：识别原文 → 润色（可回退）→ 后处理。
class PipelineTranscription : public ITranscription {
public:
    PipelineTranscription(const AppConfig& cfg, TextPolishService* polish, QObject* parent)
        : ITranscription(parent), m_config(cfg), m_polish(polish) {
        m_post.setLanguage("en");
    }

    void transcribe(const QByteArray& pcm, int, int, int) override {
        if (!m_asr.contains(pcm)) {
            emit transcriptionFinished(false, {}, {}, QStringLiteral("识别失败"));
            return;
        }
        const QString raw = m_asr.value(pcm);
        if (!m_config.polish.enableAiEnhancement) {
            finish(raw, raw);
            return;
        }
        m_polish->polishText(raw, TextPolishService::RequestParams{},
            [this, raw](bool ok, const QString& polished, const QString&) {
                const auto outcome = decideEnhancementOutcome(ok, raw, polished);
                outcome.success ? finish(raw, outcome.text)
                                : emit transcriptionFinished(false, raw, {}, QStringLiteral("润色失败"));
            });
    }

    QHash<QByteArray, QString> m_asr;

private:
    void finish(const QString& raw, const QString& text) {
        m_post.setReplaceRules(m_config.replaceRules);
        emit transcriptionFinished(true, raw, m_post.postProcess(text), {});
    }

    const AppConfig& m_config;
    TextPolishService* m_polish;
    TextPostProcessor m_post;
};

class PolishStub : public TextPolishProvider {
public:
    explicit PolishStub(bool ok) : m_ok(ok) {}
    bool supports(const TextPolishService::RequestParams&) const override { return true; }
    void polish(const QString& in, const TextPolishService::RequestParams&,
                QNetworkAccessManager*, ResultCallback done) override {
        if (m_ok) done(true, QStringLiteral("polished:%1").arg(in), {});
        else done(false, {}, QStringLiteral("boom"));
    }
private:
    bool m_ok;
};

class DictationPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        config.continuousMode = true;
        config.polish.enableAiEnhancement = false;
        config.replaceRules = QString();

        polish.setProviderFactoryForTest([this](const TextPolishService::RequestParams&) {
            return std::make_unique<PolishStub>(polishOk);
        });

        wf = new WorkflowManager(config);
        rec = new PipelineRecorder;
        model = new PipelineModel;
        asr = new PipelineTranscription(config, &polish, wf);
        rec->setParent(wf);
        model->setParent(wf);
        wf->initialize(rec, asr, model);

        // 过滤器返回 true：结果不会真的注入前台窗口，同时当作输出端收集
        wf->setInjectFilter([this](const QString& t) { injected << t; return true; });
        QObject::connect(wf, &WorkflowManager::errorOccurred,
                         [this](const QString& title, const QString&) { errors << title; });
        QObject::connect(wf, &WorkflowManager::transcriptionResultReady,
                         [this](const QString& t) { results << t; });
    }
    void TearDown() override { delete wf; }

    void hear(const QByteArray& pcm, const QString& asrText) {
        asr->m_asr.insert(pcm, asrText);
        rec->speak(pcm);
    }

    AppConfig config;
    TextPolishService polish{ nullptr };
    bool polishOk = true;
    WorkflowManager* wf = nullptr;
    PipelineRecorder* rec = nullptr;
    PipelineModel* model = nullptr;
    PipelineTranscription* asr = nullptr;
    QStringList injected;
    QStringList results;   // transcriptionResultReady 发出的结果，实时转录页从这里取稿
    QStringList errors;
};

} // namespace

TEST_F(DictationPipelineTest, UtteranceFlowsToOutputWithReplaceRulesAndPunctuation) {
    config.replaceRules = "hi=>hello";
    wf->start();
    hear("a", "hi world");
    EXPECT_EQ(injected, QStringList{ "hello world." });
    EXPECT_TRUE(errors.isEmpty());
    EXPECT_EQ(wf->pendingCount(), 0);
}

TEST_F(DictationPipelineTest, InjectingIsOnByDefault) {
    wf->start();
    hear("a", "hi world");
    EXPECT_EQ(injected, QStringList{ "hi world." });
    EXPECT_EQ(results, QStringList{ "hi world." });
}

// 前台注入关掉时过滤器仍收到结果：Google 文档输出靠它把文字发往绑定的标签页
TEST_F(DictationPipelineTest, InjectDisabledStillOffersResultToFilter) {
    wf->setInjectEnabled(false);
    wf->start();
    hear("a", "hi world");
    EXPECT_EQ(injected, QStringList{ "hi world." });
    EXPECT_EQ(results, QStringList{ "hi world." });
}

TEST_F(DictationPipelineTest, InjectToggleMidSessionKeepsEveryResult) {
    wf->start();
    wf->setInjectEnabled(false);
    hear("a", "first");
    wf->setInjectEnabled(true);
    hear("b", "second");
    EXPECT_EQ(injected, (QStringList{ "first.", "second." }));
    EXPECT_EQ(results, (QStringList{ "first.", "second." }));
}

TEST_F(DictationPipelineTest, PolishedTextIsPostProcessedAfterwards) {
    config.polish.enableAiEnhancement = true;
    config.replaceRules = "hi=>hello";
    wf->start();
    hear("a", "hi world");
    EXPECT_EQ(injected, QStringList{ "polished:hello world." });
}

TEST_F(DictationPipelineTest, PolishFailureFallsBackToRecognizedText) {
    config.polish.enableAiEnhancement = true;
    polishOk = false;
    wf->start();
    hear("a", "plain text");
    EXPECT_EQ(injected, QStringList{ "plain text." });
    EXPECT_TRUE(errors.isEmpty());
}

TEST_F(DictationPipelineTest, OutputKeepsSpeakingOrder) {
    wf->start();
    hear("a", "first");
    hear("b", "second");
    hear("c", "third");
    EXPECT_EQ(injected, (QStringList{ "first.", "second.", "third." }));
}

TEST_F(DictationPipelineTest, RecognitionFailureReportsErrorAndNextUtteranceStillWorks) {
    wf->start();
    rec->speak("unknown");   // 假 ASR 查不到 → 识别失败
    hear("b", "after failure");
    EXPECT_EQ(errors.size(), 1);
    EXPECT_EQ(injected, QStringList{ "after failure." });
}

TEST_F(DictationPipelineTest, EmptyRecognitionResultInjectsNothing) {
    wf->start();
    hear("a", "");
    EXPECT_TRUE(injected.isEmpty());
    EXPECT_TRUE(errors.isEmpty());
    hear("b", "still alive");
    EXPECT_EQ(injected, QStringList{ "still alive." });
}

TEST_F(DictationPipelineTest, RecorderErrorIsForwardedToUser) {
    wf->start();
    rec->fail(QStringLiteral("VAD 模型缺失"), QStringLiteral("请先下载"));
    EXPECT_EQ(errors, QStringList{ "VAD 模型缺失" });
}

TEST_F(DictationPipelineTest, UtteranceAfterStopIsNotTranscribed) {
    wf->start();
    wf->stop();
    hear("a", "late");
    EXPECT_TRUE(injected.isEmpty());
    EXPECT_EQ(wf->state(), WorkflowState::Idle);
}
