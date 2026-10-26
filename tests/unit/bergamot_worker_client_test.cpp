#include <gtest/gtest.h>

#include <QTemporaryFile>

#include "translation/BergamotWorkerClient.h"
#include "../../tools/translate_worker/WorkerProtocol.h"

namespace {

// 不启动真实进程：记录发给 worker 的行，响应由测试手动喂进来
class FakeClient : public BergamotWorkerClient {
public:
    using BergamotWorkerClient::BergamotWorkerClient;

    QStringList sent;
    int starts = 0;

    void feed(const QString& line) { handleLine(line); }
    void crash() { handleWorkerGone(QStringLiteral("test")); }

protected:
    void startWorker() override { ++starts; }
    void sendLine(const QByteArray& line) override { sent << QString::fromUtf8(line); }
    void stopWorker() override {}
};

struct Result {
    qint64 id = 0;
    bool ok = false;
    QString text;
    QString error;
};

class WorkerClientTest : public ::testing::Test {
protected:
    WorkerClientTest() {
        known = { "de>en", "en>de", "en>es", "es>en", "zh-Hans>en" };
        installed = { "de>en", "en>de", "en>es" };   // zh-Hans>en 在清单里但还没下载

        BergamotWorkerClient::RouteProvider provider;
        provider.plan = [this](const QString& from, const QString& to) { return planRoute(from, to, known); };
        provider.isInstalled = [this](const QString& from, const QString& to) { return installed.contains(from + ">" + to); };
        provider.configPath = [this](const QString& from, const QString& to) {
            return installed.contains(from + ">" + to) ? QStringLiteral("cfg/%1-%2.yml").arg(from, to) : QString();
        };

        exe.open();   // 存在的文件当作 worker 程序
        client = std::make_unique<FakeClient>(provider, exe.fileName());
        QObject::connect(client.get(), &ITranslationBackend::translated,
                         [this](qint64 id, bool ok, const QString& text, const QString& error) {
                             results.append({ id, ok, text, error });
                         });
    }

    QSet<QString> known;
    QSet<QString> installed;
    QTemporaryFile exe;
    std::unique_ptr<FakeClient> client;
    QList<Result> results;
};


} // namespace

TEST_F(WorkerClientTest, RequestsWaitForReadyThenLoadModelBeforeTranslating) {
    client->translate(1, QStringLiteral("Hello"), QStringLiteral("en"), QStringLiteral("de"));

    EXPECT_EQ(client->starts, 1);
    EXPECT_TRUE(client->sent.isEmpty());   // worker 还没输出 READY，先排队

    client->feed(QStringLiteral("READY"));
    ASSERT_EQ(client->sent.size(), 2);
    EXPECT_TRUE(client->sent[0].startsWith(QStringLiteral("L\ten\tde\tcfg/en-de.yml")));
    EXPECT_TRUE(client->sent[1].startsWith(QStringLiteral("T\t1\ten\tde\tHello")));
}

TEST_F(WorkerClientTest, PivotRouteLoadsBothHopsAndSendsOneRequest) {
    client->feed(QStringLiteral("READY"));
    client->translate(2, QStringLiteral("Guten Tag"), QStringLiteral("de"), QStringLiteral("es"));

    ASSERT_EQ(client->sent.size(), 3);
    EXPECT_TRUE(client->sent[0].startsWith(QStringLiteral("L\tde\ten")));
    EXPECT_TRUE(client->sent[1].startsWith(QStringLiteral("L\ten\tes")));
    EXPECT_TRUE(client->sent[2].startsWith(QStringLiteral("T\t2\tde\tes")));
}

TEST_F(WorkerClientTest, LoadedDirectionIsNotLoadedAgain) {
    client->translate(1, QStringLiteral("a"), QStringLiteral("en"), QStringLiteral("de"));
    client->feed(QStringLiteral("READY"));
    client->sent.clear();

    client->translate(2, QStringLiteral("b"), QStringLiteral("en"), QStringLiteral("de"));
    ASSERT_EQ(client->sent.size(), 1);
    EXPECT_TRUE(client->sent[0].startsWith(QStringLiteral("T\t2")));
}

TEST_F(WorkerClientTest, ResultIsMatchedByIdEvenWhenOutOfOrder) {
    client->translate(1, QStringLiteral("a"), QStringLiteral("en"), QStringLiteral("de"));
    client->translate(2, QStringLiteral("b"), QStringLiteral("en"), QStringLiteral("de"));
    client->feed(QStringLiteral("READY"));

    client->feed(QString::fromStdString(worker_protocol::formatResult(2, 10, "B\nzwei")));
    client->feed(QString::fromStdString(worker_protocol::formatResult(1, 12, "A")));

    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].id, 2);
    EXPECT_EQ(results[0].text, QStringLiteral("B\nzwei"));
    EXPECT_EQ(results[1].id, 1);
    EXPECT_TRUE(results[1].ok);
}

TEST_F(WorkerClientTest, DuplicateOrUnknownResultIsIgnored) {
    client->translate(1, QStringLiteral("a"), QStringLiteral("en"), QStringLiteral("de"));
    client->feed(QStringLiteral("READY"));

    client->feed(QString::fromStdString(worker_protocol::formatResult(1, 1, "x")));
    client->feed(QString::fromStdString(worker_protocol::formatResult(1, 1, "again")));
    client->feed(QString::fromStdString(worker_protocol::formatResult(99, 1, "stray")));

    EXPECT_EQ(results.size(), 1);
}

TEST_F(WorkerClientTest, WorkerErrorFailsThatRequest) {
    client->translate(1, QStringLiteral("a"), QStringLiteral("en"), QStringLiteral("de"));
    client->feed(QStringLiteral("READY"));
    client->feed(QString::fromStdString(worker_protocol::formatError(1, worker_protocol::kInternal, "boom")));

    ASSERT_EQ(results.size(), 1);
    EXPECT_FALSE(results[0].ok);
    EXPECT_TRUE(results[0].error.contains(QStringLiteral("boom")));
}

TEST_F(WorkerClientTest, NoModelErrorMakesNextRequestReloadTheModel) {
    client->translate(1, QStringLiteral("a"), QStringLiteral("en"), QStringLiteral("de"));
    client->feed(QStringLiteral("READY"));
    client->feed(QString::fromStdString(worker_protocol::formatError(1, worker_protocol::kNoModel, "gone")));
    client->sent.clear();

    client->translate(2, QStringLiteral("b"), QStringLiteral("en"), QStringLiteral("de"));
    ASSERT_EQ(client->sent.size(), 2);
    EXPECT_TRUE(client->sent[0].startsWith(QStringLiteral("L\ten\tde")));
}

TEST_F(WorkerClientTest, CrashFailsEveryPendingRequestAndNextRequestRestarts) {
    client->feed(QStringLiteral("READY"));
    client->translate(1, QStringLiteral("a"), QStringLiteral("en"), QStringLiteral("de"));
    client->translate(2, QStringLiteral("b"), QStringLiteral("de"), QStringLiteral("en"));
    ASSERT_EQ(client->starts, 1);

    client->crash();
    ASSERT_EQ(results.size(), 2);
    for (const Result& r : results) EXPECT_FALSE(r.ok);

    // 下一次翻译重新启动 worker，并重新加载模型（旧进程里的模型已经没了）
    client->sent.clear();
    client->translate(3, QStringLiteral("c"), QStringLiteral("en"), QStringLiteral("de"));
    EXPECT_EQ(client->starts, 2);
    client->feed(QStringLiteral("READY"));
    ASSERT_EQ(client->sent.size(), 2);
    EXPECT_TRUE(client->sent[0].startsWith(QStringLiteral("L\ten\tde")));
}

TEST_F(WorkerClientTest, UnsupportedDirectionFailsImmediatelyWithoutStartingWorker) {
    client->translate(1, QStringLiteral("a"), QStringLiteral("en"), QStringLiteral("fr"));

    ASSERT_EQ(results.size(), 1);
    EXPECT_FALSE(results[0].ok);
    EXPECT_EQ(client->starts, 0);
}

TEST_F(WorkerClientTest, NotInstalledModelFailsWithoutSendingAnything) {
    client->feed(QStringLiteral("READY"));
    client->translate(1, QStringLiteral("a"), QStringLiteral("zh-Hans"), QStringLiteral("en"));

    ASSERT_EQ(results.size(), 1);
    EXPECT_FALSE(results[0].ok);
    EXPECT_TRUE(client->sent.isEmpty());
}

TEST_F(WorkerClientTest, AvailabilityNeedsInstalledModelsForEveryHop) {
    EXPECT_TRUE(client->isUsable());
    EXPECT_TRUE(client->isAvailable(QStringLiteral("en"), QStringLiteral("de")));
    EXPECT_TRUE(client->isAvailable(QStringLiteral("de"), QStringLiteral("es")));    // 中转两跳都已安装
    EXPECT_FALSE(client->isAvailable(QStringLiteral("zh-Hans"), QStringLiteral("en")));   // 没下载
    EXPECT_FALSE(client->isAvailable(QStringLiteral("zh-Hans"), QStringLiteral("es")));   // 其中一跳没下载
    EXPECT_FALSE(client->isAvailable(QStringLiteral("en"), QStringLiteral("fr")));        // 清单里没有
}

TEST(WorkerClientUsabilityTest, MissingWorkerExecutableIsNotUsable) {
    BergamotWorkerClient::RouteProvider provider;
    provider.plan = [](const QString&, const QString&) { return TranslationRoute{}; };
    provider.isInstalled = [](const QString&, const QString&) { return true; };
    provider.configPath = [](const QString&, const QString&) { return QString(); };

    FakeClient client(provider, QStringLiteral("Z:/definitely/missing/translate_worker.exe"));
    EXPECT_FALSE(client.isUsable());
    EXPECT_FALSE(client.isAvailable(QStringLiteral("en"), QStringLiteral("de")));
}
