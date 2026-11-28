#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <functional>
#include <memory>

#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "services/TextPolishService.h"
#include "services/textpolish/OpenWebUiProvider.h"
#include "services/textpolish/TextPolishProvider.h"

namespace {

struct Reply {
    int status = 200;
    QByteArray headers;
    QByteArray body;
};

struct Seen {
    QByteArray requestLine;
    QByteArray authorization;
    QByteArray body;
};

class FakeServer : public QObject {
public:
    FakeServer() {
        m_server.listen(QHostAddress::LocalHost);
        connect(&m_server, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket* s = m_server.nextPendingConnection()) {
                s->setParent(this);
                auto buffer = std::make_shared<QByteArray>();
                connect(s, &QTcpSocket::readyRead, this, [this, s, buffer]() {
                    buffer->append(s->readAll());
                    onData(s, *buffer);
                });
            }
        });
    }

    QString root() const { return QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort()); }
    quint16 port() const { return m_server.serverPort(); }
    void queue(const Reply& reply) { m_replies.append(reply); }
    const QList<Seen>& seen() const { return m_seen; }

private:
    void onData(QTcpSocket* s, const QByteArray& data) {
        const int headerEnd = data.indexOf("\r\n\r\n");
        if (headerEnd < 0) return;
        const QByteArray head = data.left(headerEnd);
        int length = 0;
        Seen seen;
        const QList<QByteArray> lines = head.split('\n');
        seen.requestLine = lines.value(0).trimmed();
        for (const QByteArray& line : lines) {
            const QByteArray lower = line.toLower();
            if (lower.startsWith("content-length:")) length = line.mid(15).trimmed().toInt();
            if (lower.startsWith("authorization:")) seen.authorization = line.mid(14).trimmed();
        }
        if (data.size() < headerEnd + 4 + length) return;
        if (m_done.contains(s)) return;
        m_done.insert(s);
        seen.body = data.mid(headerEnd + 4, length);
        m_seen.append(seen);

        const Reply reply = m_replies.isEmpty() ? Reply{ 500, {}, "{}" }
                                                : (m_replies.size() > 1 ? m_replies.takeFirst() : m_replies.first());
        QByteArray out = "HTTP/1.1 " + QByteArray::number(reply.status) + " X\r\n";
        out += "Content-Type: application/json\r\nConnection: close\r\n" + reply.headers;
        out += "Content-Length: " + QByteArray::number(reply.body.size()) + "\r\n\r\n" + reply.body;
        s->write(out);
        s->disconnectFromHost();
    }

    QTcpServer m_server;
    QList<Reply> m_replies;
    QList<Seen> m_seen;
    QSet<QTcpSocket*> m_done;
};

bool waitFor(const std::function<bool()>& cond, int ms = 8000) {
    QElapsedTimer t;
    t.start();
    while (!cond() && t.elapsed() < ms) QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return cond();
}

QByteArray chatReply(const QString& content, const QString& finish = QStringLiteral("stop")) {
    const QJsonObject message{ { "content", content } };
    const QJsonObject choice{ { "message", message }, { "finish_reason", finish } };
    return QJsonDocument(QJsonObject{ { "choices", QJsonArray{ choice } } }).toJson(QJsonDocument::Compact);
}

TextPolishService::RequestParams translateParams(const QString& url, const QString& key = QStringLiteral("secret")) {
    TextPolishService::RequestParams p;
    p.aiEngine = kOpenWebUiEngine;
    p.customUrl = url;
    p.apiKey = key;
    p.model = QStringLiteral("qwen3");
    p.targetLang = QStringLiteral("英文");
    p.translateOnly = true;
    return p;
}

struct Outcome {
    bool done = false;
    bool ok = false;
    QString text;
    QString error;
};

}

class OpenWebUiProviderTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!qApp) {
            static int argc = 0;
            static char* argv[] = { nullptr };
            new QCoreApplication(argc, argv);
        }
        OpenWebUiProvider::setRetryBaseDelayMs(1);
    }
    void TearDown() override { OpenWebUiProvider::setRetryBaseDelayMs(1000); }

    static QString base(const QString& url, bool selfSigned = false, QString* error = nullptr) {
        return OpenWebUiProvider::normalizeBase(url, selfSigned, error);
    }

    static Outcome runPolish(const TextPolishService::RequestParams& params, const QString& text = QStringLiteral("你好")) {
        QNetworkAccessManager nam;
        Outcome out;
        {
            OpenWebUiProvider provider(params.apiKey, params.allowSelfSigned);
            provider.polish(text, params, &nam, [&out](bool ok, QString t, QString e) {
                out = { true, ok, t, e };
            });
        }
        waitFor([&out]() { return out.done; });
        return out;
    }
};

TEST_F(OpenWebUiProviderTest, RootAddressGetsApiPrefix) {
    EXPECT_EQ(base("http://localhost:3000"), "http://localhost:3000/api");
    EXPECT_EQ(base("http://localhost:3000/"), "http://localhost:3000/api");
    EXPECT_EQ(base("  https://webui.example.com  "), "https://webui.example.com/api");
}

TEST_F(OpenWebUiProviderTest, ExplicitPathIsKept) {
    EXPECT_EQ(base("http://localhost:3000/api"), "http://localhost:3000/api");
    EXPECT_EQ(base("http://localhost:3000/api/chat/completions"), "http://localhost:3000/api");
    EXPECT_EQ(base("http://localhost:1234/v1"), "http://localhost:1234/v1");
    EXPECT_EQ(base("http://localhost:1234/v1/chat/completions/"), "http://localhost:1234/v1");
}

TEST_F(OpenWebUiProviderTest, RejectsMalformedAddresses) {
    for (const QString& bad : { QString(), QStringLiteral("ftp://host"), QStringLiteral("localhost:3000"),
                                QStringLiteral("http://user:pw@host"), QStringLiteral("http://host/?a=1"),
                                QStringLiteral("http://host/#frag") }) {
        QString error;
        EXPECT_TRUE(base(bad, false, &error).isEmpty()) << bad.toStdString();
        EXPECT_FALSE(error.isEmpty()) << bad.toStdString();
    }
}

TEST_F(OpenWebUiProviderTest, SelfSignedOnlyForLoopback) {
    QString error;
    EXPECT_FALSE(base("https://127.0.0.1:8443", true, &error).isEmpty());
    EXPECT_FALSE(base("https://localhost", true, &error).isEmpty());
    // 开关默认开启：远程地址不报错，只是不会跳过证书校验（由 isLoopback 把关）
    EXPECT_FALSE(base("https://webui.example.com", true, &error).isEmpty());
    EXPECT_FALSE(base("https://webui.example.com", false, &error).isEmpty());
}

TEST_F(OpenWebUiProviderTest, LoopbackDetectionAndTimeouts) {
    EXPECT_TRUE(OpenWebUiProvider::isLoopback("http://localhost:3000"));
    EXPECT_TRUE(OpenWebUiProvider::isLoopback("http://LOCALHOST:3000"));
    EXPECT_TRUE(OpenWebUiProvider::isLoopback("http://127.0.0.1:3000"));
    EXPECT_TRUE(OpenWebUiProvider::isLoopback("http://[::1]:3000"));
    EXPECT_FALSE(OpenWebUiProvider::isLoopback("http://192.168.1.5:3000"));
    EXPECT_FALSE(OpenWebUiProvider::isLoopback("https://webui.example.com"));
    EXPECT_EQ(OpenWebUiProvider::requestTimeoutMs("http://localhost:3000"), 300 * 1000);
    EXPECT_EQ(OpenWebUiProvider::requestTimeoutMs("https://webui.example.com"), 60 * 1000);
}

TEST_F(OpenWebUiProviderTest, ChatBodyIsTranslateOnlyAndDeterministic) {
    TextPolishService::RequestParams p = translateParams("http://localhost:3000");
    p.glossary = { { QStringLiteral("a"), QStringLiteral("b") } };
    const QJsonObject body = QJsonDocument::fromJson(OpenWebUiProvider::buildChatBody(QStringLiteral("你好"), p)).object();
    EXPECT_EQ(body.value("model").toString(), "qwen3");
    EXPECT_EQ(body.value("temperature").toDouble(-1), 0.0);
    const QJsonArray messages = body.value("messages").toArray();
    ASSERT_EQ(messages.size(), 2);
    EXPECT_EQ(messages[0].toObject().value("role").toString(), "system");
    EXPECT_EQ(messages[1].toObject().value("content").toString(), "你好");
    EXPECT_TRUE(messages[0].toObject().value("content").toString().contains(QStringLiteral("英文")));
}

TEST_F(OpenWebUiProviderTest, ChatBodyForcesTranslateOnlyEvenWhenFlagMissing) {
    TextPolishService::RequestParams p = translateParams("http://localhost:3000");
    p.translateOnly = false;
    const QJsonObject body = QJsonDocument::fromJson(OpenWebUiProvider::buildChatBody(QStringLiteral("hi"), p)).object();
    EXPECT_EQ(body.value("messages").toArray().size(), 2);
}

TEST_F(OpenWebUiProviderTest, ExtractTextStripsReasoningAndRejectsBadReplies) {
    QString error;
    EXPECT_EQ(OpenWebUiProvider::extractText(chatReply("<think>hmm</think>Hello"), &error), "Hello");
    EXPECT_TRUE(error.isEmpty());

    error.clear();
    EXPECT_TRUE(OpenWebUiProvider::extractText(chatReply("cut", "length"), &error).isEmpty());
    EXPECT_TRUE(error.contains(QStringLiteral("截断")));

    error.clear();
    EXPECT_TRUE(OpenWebUiProvider::extractText(chatReply("<think>only thinking</think>"), &error).isEmpty());
    EXPECT_TRUE(error.contains(QStringLiteral("没有返回正文")));

    error.clear();
    EXPECT_TRUE(OpenWebUiProvider::extractText(R"({"error":{"message":"boom"}})", &error).isEmpty());
    EXPECT_EQ(error, "boom");

    error.clear();
    EXPECT_TRUE(OpenWebUiProvider::extractText("<html>", &error).isEmpty());
    EXPECT_FALSE(error.isEmpty());
}

TEST_F(OpenWebUiProviderTest, ParseModelsAcceptsOpenWebUiListWithoutObjectField) {
    QString error;
    const QStringList ids = OpenWebUiProvider::parseModels(
        R"({"data":[{"id":"qwen3","name":"Qwen3"},{"id":"llama3"},{"id":"qwen3"}]})", &error);
    EXPECT_TRUE(error.isEmpty());
    EXPECT_EQ(ids, (QStringList{ "qwen3", "llama3" }));
}

TEST_F(OpenWebUiProviderTest, ParseModelsReportsEachFailureMode) {
    auto err = [](const QByteArray& json) {
        QString e;
        OpenWebUiProvider::parseModels(json, &e);
        return e;
    };
    EXPECT_TRUE(err("not json").contains(QStringLiteral("JSON")));
    EXPECT_TRUE(err(R"({"models":[]})").contains(QStringLiteral("data")));
    EXPECT_TRUE(err(R"({"data":[]})").contains(QStringLiteral("空模型列表")));
    EXPECT_TRUE(err(R"({"data":[{"name":"x"}]})").contains(QStringLiteral("模型 ID")));
    QJsonArray many;
    for (int i = 0; i < OpenWebUiProvider::kMaxModels + 1; ++i) many.append(QJsonObject{ { "id", QString::number(i) } });
    EXPECT_TRUE(err(QJsonDocument(QJsonObject{ { "data", many } }).toJson()).contains(QStringLiteral("上限")));
}

TEST_F(OpenWebUiProviderTest, RetryPlanForHttpStatuses) {
    using P = OpenWebUiProvider;
    auto plan = [](int attempt, int status, const QByteArray& body = {}, const QByteArray& retryAfter = {}) {
        return P::planRetry(attempt, 0, status, 0, body, retryAfter, 60000, false);
    };
    EXPECT_TRUE(plan(0, 401).error.contains(QStringLiteral("认证")));
    EXPECT_FALSE(plan(0, 401).retry);
    EXPECT_FALSE(plan(0, 403).retry);
    EXPECT_TRUE(plan(0, 413).error.contains(QStringLiteral("上下文")));
    EXPECT_TRUE(plan(0, 400, "Maximum Context length exceeded").error.contains(QStringLiteral("上下文")));
    EXPECT_TRUE(plan(0, 400, "bad request").error.contains(QStringLiteral("HTTP 400")));
    EXPECT_TRUE(plan(0, 404).error.contains(QStringLiteral("HTTP 404")));
    EXPECT_TRUE(plan(0, 301).error.contains(QStringLiteral("HTTP 301")));

    for (int status : { 408, 429, 500, 502, 503, 504 }) EXPECT_TRUE(plan(0, status).retry) << status;
    EXPECT_FALSE(plan(3, 503).retry);
    EXPECT_TRUE(plan(3, 503).error.contains(QStringLiteral("重试 3 次")));
    EXPECT_EQ(plan(0, 429, {}, "5").delayMs, 5000);
    EXPECT_EQ(plan(0, 429, {}, "999").delayMs, 60000);
    EXPECT_EQ(plan(0, 503, {}, "soon").delayMs, plan(0, 503).delayMs);
    EXPECT_EQ(plan(2, 503).delayMs, plan(0, 503).delayMs * 4);
}

TEST_F(OpenWebUiProviderTest, RetryPlanForNetworkErrors) {
    using P = OpenWebUiProvider;
    auto plan = [](int attempt, int timeouts, QNetworkReply::NetworkError e, bool loopback = false) {
        return P::planRetry(attempt, timeouts, 0, int(e), {}, {}, 60000, loopback);
    };
    EXPECT_FALSE(plan(0, 0, QNetworkReply::ConnectionRefusedError).retry);
    EXPECT_TRUE(plan(0, 0, QNetworkReply::ConnectionRefusedError).error.contains(QStringLiteral("连接被拒绝")));
    EXPECT_FALSE(plan(0, 0, QNetworkReply::HostNotFoundError).retry);

    const auto tls = plan(0, 0, QNetworkReply::SslHandshakeFailedError, true);
    EXPECT_FALSE(tls.retry);
    EXPECT_TRUE(tls.error.contains(QStringLiteral("自签名")));
    EXPECT_FALSE(plan(0, 0, QNetworkReply::SslHandshakeFailedError, false).error.contains(QStringLiteral("自签名")));

    const auto firstTimeout = plan(0, 0, QNetworkReply::OperationCanceledError);
    EXPECT_TRUE(firstTimeout.retry);
    EXPECT_EQ(firstTimeout.timeouts, 1);
    const auto secondTimeout = plan(1, 1, QNetworkReply::TimeoutError);
    EXPECT_FALSE(secondTimeout.retry);
    EXPECT_TRUE(secondTimeout.error.contains(QStringLiteral("60")));

    EXPECT_TRUE(plan(0, 0, QNetworkReply::RemoteHostClosedError).retry);
    EXPECT_FALSE(plan(3, 0, QNetworkReply::RemoteHostClosedError).retry);
}

TEST_F(OpenWebUiProviderTest, CapabilitiesAreTranslateOnly) {
    TextPolishService service(nullptr);
    const auto caps = service.capabilities(kOpenWebUiEngine);
    EXPECT_FALSE(caps.canPolish);
    EXPECT_TRUE(caps.canTranslate);
    EXPECT_TRUE(caps.canTest);
    EXPECT_TRUE(caps.canFetchModels);
}

TEST_F(OpenWebUiProviderTest, ConfigHelpersForEngine) {
    PolishConfig p;
    p.aiEngineIndex = kOpenWebUiEngine;
    EXPECT_TRUE(p.translationOnlyEngine());
    EXPECT_FALSE(p.usesLocalTranslation());
    EXPECT_FALSE(p.sendsGlossaryToTranslator());
    EXPECT_FALSE(p.canChooseTranslateModel());
    EXPECT_FALSE(p.cloudEngineConfigured());
    p.apiUrl = "http://localhost:3000";
    EXPECT_TRUE(p.cloudEngineConfigured());
    p.model = "qwen3";
    p.translateModel = "other";
    EXPECT_EQ(p.effectiveTranslateModel(), "qwen3");

    p.aiEngineIndex = 1;
    EXPECT_FALSE(p.translationOnlyEngine());
    EXPECT_TRUE(p.sendsGlossaryToTranslator());
    p.aiEngineIndex = kLocalTranslationEngine;
    EXPECT_TRUE(p.translationOnlyEngine());
}

TEST_F(OpenWebUiProviderTest, SelfSignedFlagSurvivesConfigRoundTrip) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString oldPath = ConfigManager::configFilePath();
    ConfigManager::setConfigFilePath(dir.filePath("ime.ini"));
    {
        ConfigManager writer;
        writer.load();
        writer.config().polish.allowSelfSigned = true;
        ASSERT_TRUE(writer.save());
    }
    ConfigManager reader;
    reader.load();
    EXPECT_TRUE(reader.config().polish.allowSelfSigned);
    ConfigManager::setConfigFilePath(oldPath);
}

TEST_F(OpenWebUiProviderTest, ChatRetriesTransientErrorThenReturnsCleanText) {
    FakeServer server;
    server.queue({ 503, "Retry-After: 0\r\n", "{}" });
    server.queue({ 200, {}, chatReply("<think>x</think>Hello") });
    const Outcome out = runPolish(translateParams(server.root()));
    ASSERT_TRUE(out.done);
    EXPECT_TRUE(out.ok) << out.error.toStdString();
    EXPECT_EQ(out.text, "Hello");
    ASSERT_EQ(server.seen().size(), 2);
    EXPECT_EQ(server.seen()[0].requestLine, "POST /api/chat/completions HTTP/1.1");
    EXPECT_EQ(server.seen()[1].authorization, "Bearer secret");
    EXPECT_EQ(QJsonDocument::fromJson(server.seen()[1].body).object().value("model").toString(), "qwen3");
}

TEST_F(OpenWebUiProviderTest, ChatAuthFailureIsNotRetried) {
    FakeServer server;
    server.queue({ 401, {}, R"({"detail":"nope"})" });
    const Outcome out = runPolish(translateParams(server.root()));
    ASSERT_TRUE(out.done);
    EXPECT_FALSE(out.ok);
    EXPECT_TRUE(out.error.contains(QStringLiteral("认证")));
    EXPECT_EQ(server.seen().size(), 1);
}

TEST_F(OpenWebUiProviderTest, ChatGivesUpAfterFourAttempts) {
    FakeServer server;
    server.queue({ 502, {}, "{}" });
    const Outcome out = runPolish(translateParams(server.root()));
    ASSERT_TRUE(out.done);
    EXPECT_FALSE(out.ok);
    EXPECT_EQ(server.seen().size(), OpenWebUiProvider::kMaxAttempts);
    EXPECT_TRUE(out.error.contains(QStringLiteral("重试 3 次")));
}

TEST_F(OpenWebUiProviderTest, ChatDoesNotFollowRedirects) {
    FakeServer server;
    server.queue({ 302, "Location: http://127.0.0.1:1/steal\r\n", "" });
    const Outcome out = runPolish(translateParams(server.root()));
    ASSERT_TRUE(out.done);
    EXPECT_FALSE(out.ok);
    EXPECT_EQ(server.seen().size(), 1);
    EXPECT_TRUE(out.error.contains(QStringLiteral("HTTP 302")));
}

TEST_F(OpenWebUiProviderTest, ChatReportsRefusedConnection) {
    quint16 port = 0;
    {
        QTcpServer probe;
        ASSERT_TRUE(probe.listen(QHostAddress::LocalHost));
        port = probe.serverPort();
    }
    const Outcome out = runPolish(translateParams(QStringLiteral("http://127.0.0.1:%1").arg(port)));
    ASSERT_TRUE(out.done);
    EXPECT_FALSE(out.ok);
    EXPECT_TRUE(out.error.contains(QStringLiteral("连接被拒绝"))) << out.error.toStdString();
}

TEST_F(OpenWebUiProviderTest, PolishWithoutTranslateOnlyPassesTextThrough) {
    FakeServer server;
    TextPolishService::RequestParams p = translateParams(server.root());
    p.translateOnly = false;
    const Outcome out = runPolish(p, QStringLiteral("原文"));
    ASSERT_TRUE(out.done);
    EXPECT_TRUE(out.ok);
    EXPECT_EQ(out.text, "原文");
    EXPECT_TRUE(server.seen().isEmpty());
}

TEST_F(OpenWebUiProviderTest, PolishValidatesAddressAndModelBeforeSending) {
    TextPolishService::RequestParams p = translateParams(QString());
    EXPECT_FALSE(runPolish(p).ok);

    FakeServer server;
    p = translateParams(server.root());
    p.model.clear();
    const Outcome out = runPolish(p);
    EXPECT_FALSE(out.ok);
    EXPECT_TRUE(out.error.contains(QStringLiteral("模型")));
    EXPECT_TRUE(server.seen().isEmpty());
}

TEST_F(OpenWebUiProviderTest, FetchModelsHitsApiModelsWithBearerKey) {
    FakeServer server;
    server.queue({ 200, {}, R"({"data":[{"id":"qwen3"},{"id":"llama3"}]})" });
    QNetworkAccessManager nam;
    bool done = false, ok = false;
    QStringList models;
    {
        OpenWebUiProvider provider(QStringLiteral("secret"));
        provider.fetchModels(server.root(), &nam, [&](bool o, QStringList m, QString) { done = true; ok = o; models = m; });
    }
    ASSERT_TRUE(waitFor([&]() { return done; }));
    EXPECT_TRUE(ok);
    EXPECT_EQ(models, (QStringList{ "qwen3", "llama3" }));
    ASSERT_EQ(server.seen().size(), 1);
    EXPECT_EQ(server.seen()[0].requestLine, "GET /api/models HTTP/1.1");
    EXPECT_EQ(server.seen()[0].authorization, "Bearer secret");
}

TEST_F(OpenWebUiProviderTest, FetchModelsMapsAuthFailure) {
    FakeServer server;
    server.queue({ 401, {}, "{}" });
    QNetworkAccessManager nam;
    bool done = false;
    QString error;
    {
        OpenWebUiProvider provider;
        provider.fetchModels(server.root(), &nam, [&](bool, QStringList, QString e) { done = true; error = e; });
    }
    ASSERT_TRUE(waitFor([&]() { return done; }));
    EXPECT_TRUE(error.contains(QStringLiteral("API Key")));
}

TEST_F(OpenWebUiProviderTest, ConnectionTestReportsModelCount) {
    FakeServer server;
    server.queue({ 200, {}, R"({"data":[{"id":"a"},{"id":"b"},{"id":"c"}]})" });
    QNetworkAccessManager nam;
    bool done = false, ok = false;
    QString message;
    {
        OpenWebUiProvider provider(QStringLiteral("k"));
        provider.test(translateParams(server.root(), "k"), &nam, [&](bool o, QString m) { done = true; ok = o; message = m; });
    }
    ASSERT_TRUE(waitFor([&]() { return done; }));
    EXPECT_TRUE(ok);
    EXPECT_TRUE(message.contains(QStringLiteral("3")));
}
