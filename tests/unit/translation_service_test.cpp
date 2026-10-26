#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QList>
#include <QThread>

#include <memory>

#include "TranslationService.h"
#include "translation/ITranslationBackend.h"
#include "translation/TranslationStage.h"

namespace {

struct Request {
    qint64 id = 0;
    QString text;
    QString from;
    QString to;
};

class FakeBackend : public ITranslationBackend {
public:
    bool available = true;
    QList<Request> requests;

    void translate(qint64 id, const QString& text, const QString& from, const QString& to) override {
        requests.append({ id, text, from, to });
    }

    bool isAvailable(const QString&, const QString&) const override { return available; }

    void reply(qint64 id, bool ok, const QString& text = {}, const QString& error = {}) {
        emit translated(id, ok, text, error);
    }
};

bool waitFor(const std::function<bool()>& predicate, int timeoutMs = 1000) {
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    return predicate();
}

} // namespace

TEST(TranslationServiceTest, SuccessfulResultIsForwardedByRequestId) {
    FakeBackend backend;
    TranslationService service(&backend);

    int calls = 0;
    bool ok = false;
    QString text, error;
    service.translate(QStringLiteral("hello"), QStringLiteral("en"), QStringLiteral("zh-Hans"),
                      [&](bool success, const QString& t, const QString& e) {
                          ++calls;
                          ok = success;
                          text = t;
                          error = e;
                      });

    ASSERT_EQ(backend.requests.size(), 1);
    const auto& req = backend.requests.first();
    EXPECT_EQ(req.text, QStringLiteral("hello"));
    EXPECT_EQ(req.from, QStringLiteral("en"));
    EXPECT_EQ(req.to, QStringLiteral("zh-Hans"));

    backend.reply(req.id, true, QStringLiteral("你好"));

    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(ok);
    EXPECT_EQ(text, QStringLiteral("你好"));
    EXPECT_TRUE(error.isEmpty());
}

TEST(TranslationServiceTest, BackendErrorIsReported) {
    FakeBackend backend;
    TranslationService service(&backend);

    bool ok = true;
    QString error;
    service.translate(QStringLiteral("hello"), QStringLiteral("en"), QStringLiteral("zh-Hans"),
                      [&](bool success, const QString&, const QString& e) {
                          ok = success;
                          error = e;
                      });

    ASSERT_EQ(backend.requests.size(), 1);
    backend.reply(backend.requests.first().id, false, {}, QStringLiteral("NO_MODEL"));

    EXPECT_FALSE(ok);
    EXPECT_EQ(error, QStringLiteral("NO_MODEL"));
}

TEST(TranslationServiceTest, ConcurrentResultsAreMatchedByIdNotArrivalOrder) {
    FakeBackend backend;
    TranslationService service(&backend);

    QString firstText, secondText;
    service.translate(QStringLiteral("first"), QStringLiteral("en"), QStringLiteral("zh-Hans"),
                      [&](bool, const QString& t, const QString&) { firstText = t; });
    service.translate(QStringLiteral("second"), QStringLiteral("en"), QStringLiteral("ja"),
                      [&](bool, const QString& t, const QString&) { secondText = t; });

    ASSERT_EQ(backend.requests.size(), 2);
    const qint64 firstId = backend.requests[0].id;
    const qint64 secondId = backend.requests[1].id;
    ASSERT_NE(firstId, secondId);

    backend.reply(secondId, true, QStringLiteral("second translated"));
    backend.reply(firstId, true, QStringLiteral("first translated"));

    EXPECT_EQ(firstText, QStringLiteral("first translated"));
    EXPECT_EQ(secondText, QStringLiteral("second translated"));
}

TEST(TranslationServiceTest, TimeoutCallsBackOnlyOnceAndDropsLateResult) {
    FakeBackend backend;
    TranslationService service(&backend);
    service.setRequestTimeoutForTest(10);

    int calls = 0;
    bool ok = true;
    QString error;
    service.translate(QStringLiteral("hello"), QStringLiteral("en"), QStringLiteral("zh-Hans"),
                      [&](bool success, const QString&, const QString& e) {
                          ++calls;
                          ok = success;
                          error = e;
                      });

    ASSERT_TRUE(waitFor([&]() { return calls == 1; }));
    EXPECT_FALSE(ok);
    EXPECT_EQ(error, QStringLiteral("翻译超时"));

    ASSERT_EQ(backend.requests.size(), 1);
    backend.reply(backend.requests.first().id, true, QStringLiteral("late"));
    QCoreApplication::processEvents();
    EXPECT_EQ(calls, 1);
}

TEST(TranslationServiceTest, BackendDestructionFailsAllPendingRequests) {
    auto* backend = new FakeBackend;
    TranslationService service(backend);

    int calls = 0;
    QString error;
    auto cb = [&](bool success, const QString&, const QString& e) {
        ++calls;
        EXPECT_FALSE(success);
        error = e;
    };
    service.translate(QStringLiteral("one"), QStringLiteral("en"), QStringLiteral("zh-Hans"), cb);
    service.translate(QStringLiteral("two"), QStringLiteral("en"), QStringLiteral("ja"), cb);
    ASSERT_EQ(backend->requests.size(), 2);

    delete backend;

    EXPECT_EQ(calls, 2);
    EXPECT_EQ(error, QStringLiteral("翻译后端已断开"));
}

TEST(TranslationServiceTest, UnavailableModelFailsWithoutSendingRequest) {
    FakeBackend backend;
    backend.available = false;
    TranslationService service(&backend);

    bool ok = true;
    QString error;
    service.translate(QStringLiteral("hello"), QStringLiteral("en"), QStringLiteral("zh-Hans"),
                      [&](bool success, const QString&, const QString& e) {
                          ok = success;
                          error = e;
                      });

    EXPECT_FALSE(ok);
    EXPECT_EQ(error, QStringLiteral("翻译模型未安装"));
    EXPECT_TRUE(backend.requests.isEmpty());
}

TEST(TranslationStageTest, SkipsFileModeCloudEngineAndNoTarget) {
    EXPECT_FALSE(decideTranslationStage(true, 1, QStringLiteral("auto"), QStringLiteral("英文")).shouldTranslate);
    EXPECT_FALSE(decideTranslationStage(false, 0, QStringLiteral("auto"), QStringLiteral("英文")).shouldTranslate);
    EXPECT_FALSE(decideTranslationStage(false, 1, QStringLiteral("auto"), {}).shouldTranslate);
    EXPECT_FALSE(decideTranslationStage(false, 1, QStringLiteral("auto"), QStringLiteral("不翻译")).shouldTranslate);
}

TEST(TranslationStageTest, SkipsSameLanguageAndKeepsRouteForLocalEngine) {
    // 源语言 auto 时按识别语言推断，推断出来与目标相同就不翻译；配置里的中文显示名先转成代码
    EXPECT_FALSE(decideTranslationStage(false, 1, QStringLiteral("en"), QStringLiteral("英文")).shouldTranslate);
    EXPECT_FALSE(decideTranslationStage(false, 1, QStringLiteral("auto"), QStringLiteral("英文"), QStringLiteral("en")).shouldTranslate);

    const auto d = decideTranslationStage(false, 1, QStringLiteral("auto"), QStringLiteral("英文"), QStringLiteral("zh"));
    EXPECT_TRUE(d.shouldTranslate);
    EXPECT_EQ(d.from, QStringLiteral("zh-Hans"));
    EXPECT_EQ(d.to, QStringLiteral("en"));

    // 其余语言直接存代码
    const auto de = decideTranslationStage(false, 1, QStringLiteral("ja"), QStringLiteral("de"));
    EXPECT_EQ(de.from, QStringLiteral("ja"));
    EXPECT_EQ(de.to, QStringLiteral("de"));
}

TEST(TranslationStageTest, OutcomeUsesTranslationOrFallsBackToOriginal) {
    const auto ok = decideTranslationOutcome(true, QStringLiteral("译文"), QStringLiteral("原文"));
    EXPECT_FALSE(ok.usedFallback);
    EXPECT_EQ(ok.text, QStringLiteral("译文"));

    const auto failed = decideTranslationOutcome(false, {}, QStringLiteral("原文"));
    EXPECT_TRUE(failed.usedFallback);
    EXPECT_EQ(failed.text, QStringLiteral("原文"));
}
